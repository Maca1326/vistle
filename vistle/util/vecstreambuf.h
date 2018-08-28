#ifndef VECSTREAMBUF_H
#define VECSTREAMBUF_H

#include <streambuf>
#include <vector>
#include <cstring>

namespace vistle {

template<typename CharT, typename TraitsT = std::char_traits<CharT> >
class vecostreambuf: public std::basic_streambuf<CharT, TraitsT> {

 public:
   vecostreambuf() {
   }

   int overflow(int ch) {
      if (ch != EOF) {
         m_vector.push_back(ch);
         return 0;
      } else {
         return EOF;
      }
   }

   std::streamsize xsputn (const CharT *s, std::streamsize num) {
      size_t oldsize = m_vector.size();
      if (oldsize+num > m_vector.capacity()) {
          m_vector.reserve(std::max(2*(oldsize+num), m_vector.capacity()*2));
      }
      m_vector.resize(oldsize+num);
      auto cur = m_vector.data()+oldsize;
      switch (num) {
          case 1 : std::memcpy(cur, s, 1) ; break;
          case 2 : std::memcpy(cur, s, 2) ; break;
          case 3 : std::memcpy(cur, s, 3) ; break;
          case 4 : std::memcpy(cur, s, 4) ; break;
          case 5 : std::memcpy(cur, s, 5) ; break;
          case 6 : std::memcpy(cur, s, 6) ; break;
          case 7 : std::memcpy(cur, s, 7) ; break;
          case 8 : std::memcpy(cur, s, 8) ; break;
          case 9 : std::memcpy(cur, s, 9) ; break;
#if defined(__GNUC__) && defined(__SIZEOF_INT128__) // hack for detect int128 support
          case 16: std::memcpy(cur, s, 16); break;
          case 17: std::memcpy(cur, s, 17); break;
#endif
          default: std::memcpy(cur, s, num);
      }
      return num;
   }

   std::size_t write(const void *ptr, std::size_t size) {
       return xsputn(static_cast<const CharT *>(ptr), size);
   }

   const std::vector<CharT> &get_vector() const {
      return m_vector;
   }

   std::vector<CharT> &get_vector() {
      return m_vector;
   }

 private:
   std::vector<CharT> m_vector;
};

template<typename CharT, typename TraitsT = std::char_traits<CharT> >
class vecistreambuf: public std::basic_streambuf<CharT, TraitsT> {

 public:
   vecistreambuf(const std::vector<CharT> &vec)
   : vec(vec)
   {
      auto &v = const_cast<std::vector<CharT> &>(vec);
      this->setg(v.data(), v.data(), v.data()+v.size());
   }

   std::size_t read(void *ptr, std::size_t size) {
       if (cur+size > vec.size())
           size = vec.size()-cur;
       const void *d = vec.data()+cur;
       switch (size) {
           case 1 : std::memcpy(ptr, d, 1) ; break;
           case 2 : std::memcpy(ptr, d, 2) ; break;
           case 3 : std::memcpy(ptr, d, 3) ; break;
           case 4 : std::memcpy(ptr, d, 4) ; break;
           case 5 : std::memcpy(ptr, d, 5) ; break;
           case 6 : std::memcpy(ptr, d, 6) ; break;
           case 7 : std::memcpy(ptr, d, 7) ; break;
           case 8 : std::memcpy(ptr, d, 8) ; break;
           case 9 : std::memcpy(ptr, d, 9) ; break;
#if defined(__GNUC__) && defined(__SIZEOF_INT128__) // hack for detect int128 support
           case 16: std::memcpy(ptr, d, 16); break;
           case 17: std::memcpy(ptr, d, 17); break;
#endif
           default: std::memcpy(ptr, d, size);
       }
       cur += size;
       return size;
   }

   bool empty() const { return cur==0; }
   CharT peekch() const { return vec[cur]; }
   CharT getch() { return vec[cur++]; }
   void ungetch(char) { if (cur>0) --cur; }

private:
   const std::vector<CharT> &vec;
   size_t cur = 0;
};

} // namespace vistle
#endif
