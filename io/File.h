void initLLFS(FILE* vdisk);

typedef struct log {
  short iNum;
  char* block;
  unsigned pos;
  unsigned size;
} log;

short file_seek(FILE* vdisk, log* l, unsigned pos);

void file_writeChar(log* l, char c);

char file_readChar(log l);

void file_writeStr(FILE* vdisk, log* l, char* str, unsigned length);

void file_readStr(FILE* vdisk, log* l, unsigned length, char* str);

void file_mkdir(FILE* vdisk, char pathString[4*31]);

void file_rmDir(FILE* vdisk, char pathString[4*31]);

log file_open(FILE* vdisk, char pathString[4*31]);

void file_close(FILE* vdisk, log* l);

void file_rm(FILE* vdisk, char pathString[4*31]);

void file_show(FILE* vdisk);
