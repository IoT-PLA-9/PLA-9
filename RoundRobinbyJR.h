  

String ReadLine(char* filename, int x = 1);

void RemoveOldLogs (char* filename, int trgr, int x);

void SaveLogs (char* filename, int trgr);

void PrintFile(char* filename);

int NumberOfLogs(char* filename);

void CopyFile(char* filename1, char* filename2);

void WriteLine(char* filename, String line, int numLine);

void EraseLastLog (char* filename);

void EraseFirstLog (char* filename, int nlogs);
