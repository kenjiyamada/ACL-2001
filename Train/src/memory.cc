//
// Process Info (CPU & Memory Usage)
//         (taken from Decode3/main.cc)
//     

#include <string>
#include <vector>
#include <fstream>
#include <strstream>

extern "C" {
#include <sys/times.h>
}

double
UTime() {
  struct tms tbuf;
  times(&tbuf);
  return ((double)tbuf.tms_utime/CLK_TCK);
}

#ifdef sun

extern "C" {
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
}

int 
MemSize()
{
  struct stat buf;
  int pid = getpid();
  char fname[32];
  sprintf(fname,"/proc/%d",pid);
  stat(fname,&buf);
  return buf.st_size;
}

#else // not sun (Linux)

extern "C" {
#include <stdio.h>
#include <unistd.h>
}

int
MemSize()
{
  int pid = getpid();
  char fname[32];
  sprintf(fname,"/proc/%d/status",pid);
  ifstream in(fname);
  if (!in) {
    cerr << "cannot open [" + string(fname) + "]\n";
    exit(1);
  }

  string VmSize;

  string line,w; vector<string> item;
  while(getline(in,line)) {
    istrstream ist(line.c_str());
    item.clear();
    while(ist>>w) item.push_back(w);
    if (item.size()>0) {
      if (item[0]=="VmSize:") {
	VmSize = item[1]; break;
      }
    }
  }
  int ret = atoi(VmSize.c_str());
  return ret;
}
#endif // sun

void
ShowMemory()
{
  cout << "MemSize=" << MemSize();
  cout << ",CPUTime=" << UTime() << "\n";
  cout.flush();
}

