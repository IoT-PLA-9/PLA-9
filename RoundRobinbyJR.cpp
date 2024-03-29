#include <SPI.h>
#include <SD.h>
 
//Modificat

void WriteLine(char* filename, String line, int numLine)
{
  File myxFile;
  
  myxFile = SD.open(filename, FILE_WRITE);
  if (numLine != NULL)
  {
    for (int i = 1; i < numLine; i++)
    {
       while (myxFile.available()) 
        line = myxFile.readStringUntil('\n');
    }
  }
  myxFile.println(line);
  myxFile.close();
}
 
String ReadLine(char* filename, int x) {

 File myxFile;
  String line;
   myxFile = SD.open(filename);

    for (int i = 1; i < x; i++)
    {
       while (myxFile.available()) 
        line = myxFile.readStringUntil('\n');
    }
   return line; 
  /*
  for (int i = 1; i < x; i++) {
    while (myxFile.available()) {
      if (myxFile.read() == '\n' ) {
        break;
      }
    }
  }
  while (myxFile.available()) {
    char c = myxFile.read();
    line += c;
    if (c == '\n' ) {
      break;
    }
  }
  myxFile.close();
  return line;
  */
}
 
// Modificat

int NumberOfLogs(char* filename) {
  File myxFile;
  int i = 0;
  myxFile = SD.open(filename);
  while (myxFile.available()) {
    myxFile.readStringUntil('\n');
    i++;
  }
  myxFile.close();
  return i;   
/*
    if (myxFile.read() == '\n' ) {
      i++;
    }
*/
}
 
void CopyFile(char* filename1, char* filename2) {
  SD.remove(filename2);
  int x = NumberOfLogs(filename1);
  for (int i = 1; i <= x; i++) {
    WriteLine(filename2, ReadLine(filename1 , i), NULL);
  }
}
 
void PrintFile(char* filename) {
  File myxFile;
  myxFile = SD.open(filename);
  if (myxFile) {
    while (myxFile.available()) {
      Serial.write(myxFile.read());
    }
    myxFile.close();
  } else {
    Serial.println("Can't show content, error opening file");
  }
}
 
void RemoveOldLogs (char* filename, int trgr, int x) {
  SD.remove((char*)"tempfile.txt");
  int y = NumberOfLogs(filename);
  for (int i = (y - trgr + x + 1) ; i <= y; i++) {
    WriteLine((char*)"tempfile.txt", ReadLine(filename, i), NULL );
  }
  CopyFile((char*)"tempfile.txt", filename);
  SD.remove((char*)"tempfile.txt");
}
 
void SaveLogs (char* filename, int trgr) {
  SD.remove((char*)"tempfile.txt");
  int y = NumberOfLogs(filename);
  for (int i = (y - trgr + 1) ; i <= y; i++) {
    WriteLine((char*)(char*)"tempfile.txt", ReadLine(filename, i), NULL );
  }
  CopyFile((char*)"tempfile.txt", filename);
  SD.remove((char*)"tempfile.txt");
}

void EraseLastLog (char* filename) {
  SD.remove((char*)"tempfile.txt");
  int y = NumberOfLogs(filename);
  for (int i = 1 ; i <= y-1; i++) {
    WriteLine((char*)(char*)"tempfile.txt", ReadLine(filename, i), NULL );
  }
  CopyFile((char*)"tempfile.txt", filename);
  SD.remove((char*)"tempfile.txt");
}
