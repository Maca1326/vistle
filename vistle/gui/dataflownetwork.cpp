/*********************************************************************************/
/** \file scene.cpp
 *
 * The brunt of the graphical processing and UI interaction happens here --
 * - modules and connections are created and modified
 * - the brunt of the event handling is done
 * - any interactions between modules, such as sorting, is performed
 */
/**********************************************************************************/
#include "dataflownetwork.h"
#include "module.h"
#include "connection.h"

#include <core/statetracker.h>

#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>

namespace gui {

/*!
 * \brief Scene::Scene
 * \param parent
 */
DataFlowNetwork::DataFlowNetwork(vistle::VistleConnection *conn, QObject *parent)
: QGraphicsScene(parent)
, m_Line(nullptr)
, startPort(nullptr)
, startModule(nullptr)
, endModule(nullptr)
, m_vistleConnection(conn)
{
    // Initialize starting scene information.
    m_LineColor = Qt::black;
    m_mousePressed = false;

    m_highlightColor = QColor(200, 50, 200);
}

/*!
 * \brief Scene::~Scene
 */
DataFlowNetwork::~DataFlowNetwork()
{
    m_moduleList.clear();
}

/*!
 * \brief Scene::addModule add a module to the draw area.
 * \param modName
 * \param dropPos
 */
void DataFlowNetwork::addModule(int hub, QString modName, QPointF dropPos)
{
    Module *module = new Module(0, modName);
    ///\todo improve how the data such as the name is set in the module.
    addItem(module);
    module->setPos(dropPos);
    module->setPositionValid();
    module->setStatus(Module::SPAWNING);

    vistle::message::Spawn spawnMsg(hub, 0, modName.toUtf8().constData());
    module->setSpawnUuid(spawnMsg.uuid());
    m_vistleConnection->sendMessage(spawnMsg);

    ///\todo add the objects only to the map (sortMap) currently used for sorting, not to the list.
    ///This will remove the need for moduleList altogether
    m_moduleList.append(module);
}

void DataFlowNetwork::addModule(int moduleId, const boost::uuids::uuid &spawnUuid, QString name)
{
   //std::cerr << "addModule: name=" << name.toStdString() << ", id=" << moduleId << std::endl;
   Module *mod = findModule(spawnUuid);
   if (mod) {
      mod->sendPosition();
   } else {
      mod = findModule(moduleId);
   }
   if (!mod) {
      mod = new Module(nullptr, name);
      addItem(mod);
      mod->setStatus(Module::SPAWNING);
      m_moduleList.append(mod);
   }

   mod->setId(moduleId);
   mod->setName(QString("%1_%2").arg(name, QString::number(moduleId)));
}

void DataFlowNetwork::deleteModule(int moduleId)
{
   Module *m = findModule(moduleId);
   if (m) {
      removeItem(m);
      m_moduleList.removeAll(m);
   }
}

void DataFlowNetwork::moduleStateChanged(int moduleId, int stateBits)
{
   if (Module *m = findModule(moduleId)) {
      if (stateBits & vistle::StateObserver::Killed)
         m->setStatus(Module::KILLED);
      else if (stateBits & vistle::StateObserver::Busy)
         m->setStatus(Module::BUSY);
      else if (stateBits & vistle::StateObserver::Initialized)
         m->setStatus(Module::INITIALIZED);
      else
         m->setStatus(Module::SPAWNING);
   }
}

void DataFlowNetwork::newPort(int moduleId, QString portName)
{
   if (Module *m = findModule(moduleId)) {
      vistle::Port *port = m_vistleConnection->ui().state().portTracker()->getPort(moduleId, portName.toStdString());
      if (port) {
         m->addPort(port);
      }
   }
}

void DataFlowNetwork::newConnection(int fromId, QString fromName,
                                   int toId, QString toName) {

#if 0
   QString text = "New Connection: " + QString::number(fromId) + ":" + fromName + " -> " + QString::number(toId) + ":" + toName;
   m_console->appendDebug(text);
#endif

   vistle::Port *portFrom = m_vistleConnection->ui().state().portTracker()->getPort(fromId, fromName.toStdString());
   vistle::Port *portTo = m_vistleConnection->ui().state().portTracker()->getPort(toId, toName.toStdString());

   Module *mFrom = findModule(fromId);
   Module *mTo = findModule(toId);

   if (mFrom && portFrom && mTo && portTo) {
      addConnection(mFrom->getGuiPort(portFrom), mTo->getGuiPort(portTo));
   }
}

void DataFlowNetwork::deleteConnection(int fromId, QString fromName,
                                      int toId, QString toName)
{
#if 0
   QString text = "Connection removed: " + QString::number(fromId) + ":" + fromName + " -> " + QString::number(toId) + ":" + toName;
   m_console->appendDebug(text);
#endif

   vistle::Port *portFrom = m_vistleConnection->ui().state().portTracker()->getPort(fromId, fromName.toStdString());
   vistle::Port *portTo = m_vistleConnection->ui().state().portTracker()->getPort(toId, toName.toStdString());

   Module *mFrom = findModule(fromId);
   Module *mTo = findModule(toId);

   if (mFrom && portFrom && mTo && portTo) {
      removeConnection(mFrom->getGuiPort(portFrom), mTo->getGuiPort(portTo));
   }
}



void DataFlowNetwork::addConnection(Port *portFrom, Port *portTo, bool sendToController) {

   assert(portFrom);
   assert(portTo);

   ConnectionKey key(portFrom, portTo);
   auto it = m_connections.find(key);
   Connection *c = nullptr;
   if (it != m_connections.end()) {
      c = it->second;
   } else {
      c = new Connection(portFrom, portTo, sendToController ? Connection::ToEstablish : Connection::Established);
      m_connections[key] = c;
      addItem(c);
   }

   if (sendToController) {

      vistle::Port *vFrom = portFrom->module()->getVistlePort(portFrom);
      vistle::Port *vTo = portTo->module()->getVistlePort(portTo);
      m_vistleConnection->connect(vFrom, vTo);
   } else {
      c->setState(Connection::Established);
   }
}

void DataFlowNetwork::removeConnection(Port *portFrom, Port *portTo, bool sendToController)
{
   ConnectionKey key(portFrom, portTo);
   auto it = m_connections.find(key);
   if (it == m_connections.end()) {
      std::cerr << "connection to be removed not found" << std::endl;
      return;
   }
   Connection *c = it->second;

   if (sendToController) {
      c->setState(Connection::ToRemove);
      vistle::Port *vFrom = portFrom->module()->getVistlePort(portFrom);
      vistle::Port *vTo = portTo->module()->getVistlePort(portTo);
      m_vistleConnection->disconnect(vFrom, vTo);
   } else {
      m_connections.erase(it);
      removeItem(c);
   }
}

Module *DataFlowNetwork::findModule(int id) const
{
   for (Module *mod: m_moduleList) {
      if (mod->id() == id) {
         return mod;
      }
   }

   return nullptr;
}

Module *DataFlowNetwork::findModule(const boost::uuids::uuid &spawnUuid) const
{
   for (Module *mod: m_moduleList) {
      if (mod->spawnUuid() == spawnUuid) {
         return mod;
      }
   }

   return nullptr;
}

QColor DataFlowNetwork::highlightColor() const {

   return m_highlightColor;
}

///\todo an exception is very occasionally thrown upon a simple click inside a module's port.
///\todo left clicking inside a module's context menu still sends the left click event to the scene, but creates
///a segfault when the connection is completed

/*!
 * \brief Scene::mousePressEvent
 * \param event
 *
 * \todo test connection drawing and unforseen events more thoroughly
 */
void DataFlowNetwork::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    ///\todo add other button support
    if (event->button() == Qt::RightButton) {
        ///\todo open a context menu?
    }

    // store the click location
    vLastPoint = event->scenePos();
    // set the click flag
    m_mousePressed = true;

    // If the user clicks on a module, test for what is being clicked on.
    //  If okay, begin the process of drawing a line.
    // See what has been selected from an object, if anything.
    QGraphicsItem *item = itemAt(event->scenePos(), QTransform());
    startPort = dynamic_cast<gui::Port *>(item);
    ///\todo add other objects and dynamic cast checks here
    ///\todo dynamic cast is not a perfect solution, is there a better one?
    if (startPort) {
       // Test for port type
       switch (startPort->portType()) {
          case Port::Input:
          case Port::Output:
          case Port::Parameter:
             m_Line = new QGraphicsLineItem(QLineF(event->scenePos(),
                      event->scenePos()));
             m_Line->setPen(QPen(m_LineColor, 2));
             addItem(m_Line);
             startModule = dynamic_cast<Module *>(startPort->parentItem());
             break;
       } //end switch
    } //end if (startPort)

    QGraphicsScene::mousePressEvent(event);
}

/*!
 * \brief Scene::mouseReleaseEvent watches for click events
 * \param event
 */
void DataFlowNetwork::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem *item;
    // if there was a click
    if ((m_mousePressed) && (event->scenePos() == vLastPoint)) {
        item = itemAt(event->scenePos(), QTransform());
        if (item) {
            if (Connection *connection = dynamic_cast<Connection *>(item)) {
               removeConnection(connection->source(), connection->destination(), true);
            }
        } //end if (item)
    // if there was not a click, and if m_line already was drawn
    } else if (m_mousePressed && m_Line) {

       // clean up connection
       removeItem(m_Line);
       delete m_Line;
       m_Line = nullptr;

       // Begin testing for the finish of the line draw.
       item = itemAt(event->scenePos(), QTransform());
       if (Port *endPort = dynamic_cast<Port *>(item)) {
          // Test over the port types
          ///\todo improve testing for viability of connection
          switch (endPort->portType()) {
          case Port::Input:
             if (startPort->portType() == Port::Output) {
                endModule = dynamic_cast<Module *>(endPort->parentItem());
                if (startModule != endModule) {
                   addConnection(startPort, endPort, true);
                }
             }
             break;
          case Port::Output:
             if (startPort->portType() == Port::Input) {
                endModule = dynamic_cast<Module *>(endPort->parentItem());
                if (startModule != endModule) {
                   addConnection(startPort, endPort, true);
                }
             }
             break;
          case Port::Parameter:
             if (startPort->portType() == Port::Parameter) {
                endModule = dynamic_cast<Module *>(endPort->parentItem());
                if (startModule != endModule) {
                   addConnection(startPort, endPort, true);
                }
             }
             break;
          }
       }
    }

    // Clear data
    startModule = nullptr;
    endModule = nullptr;
    startPort = nullptr;
    m_mousePressed = false;
    QGraphicsScene::mouseReleaseEvent(event);
}

/*!
 * \brief Scene::mouseMoveEvent
 * \param event
 */
void DataFlowNetwork::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!startPort) {
       QGraphicsScene::mouseMoveEvent(event);
       return;
    }
    Port::Type port = startPort->portType();
    ///\todo should additional tests be present here?
    // if correct mode, m_line has been created, and there is a correctly initialized port:
    if (m_Line != 0
        && (port == Port::Input || port == Port::Output || port == Port::Parameter)) {
        // update the line drawing
        QLineF newLine(m_Line->line().p1(), event->scenePos());
        m_Line->setLine(newLine);
    // otherwise call standard mouse move
    } else {
        QGraphicsScene::mouseMoveEvent(event);
    }
}

} //namespace gui
