//....................................#ifndef preprocessor check for FILEWRITER_HPP macro, is it define or not.................................................. 
#ifndef FILEWRITER_HPP
//...................................if FILEWRITER_HPP macro is not define then #define preprocessor define it .................................................
#define FILEWRITER_HPP

//......................define  headers for using several function, which is included in these header files...............................
#include <Arduino.h>
#include <FS.h>

// defining a class called FileWriter
class FileWriter {
 // using private keyword to define some members of class private, so that they doesnot access outside the class.
 private:
  File file_handle;
  char _filename[15];
  char _md5[33];
  size_t _size = 0;
  bool active = false;
  bool file_open = false;
  unsigned int received_size;
  const char *tmp_filename = "tmp";
  void parse_md5_stream(MD5Builder *md5, Stream *stream);
 
 // deining some members of class public, so that they accessible outside the class using its OBJECTS
 public:
  FileWriter();
  bool Begin(const char *filename, const char *md5, size_t size);
  bool UpToDate();
  bool Open();
  bool Add(uint8_t *data, unsigned int len);
  bool Add(uint8_t *data, unsigned int len, unsigned int pos);
  bool Commit();
  void Abort();
  bool Running();
  int GetPosition();
};

//.....................................................Undefining macro FILEWRITER_HPP ......................................
#endif
