/* Host-only observation aid: remove stdio lag from one QEMU WAV capture.
 * Does not change guest code, PCM bytes, device buffering or audio clocks. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
static FILE *open_capture(const char *path, const char *mode, const char *symbol)
{
    FILE *(*real_open)(const char *, const char *)=dlsym(RTLD_NEXT,symbol);
    if (!real_open) _exit(125);
    FILE *stream=real_open(path,mode);
    const char *capture=getenv("RP_WAV_PATH");
    if (stream && capture && !strcmp(path,capture) && mode[0]=='w') {
        if (setvbuf(stream,NULL,_IONBF,0)) _exit(125);
        const char *proof=getenv("RP_WAV_PROOF");
        if (proof) {
            char text[4096];
            int size=snprintf(text,sizeof(text),"PID=%ld\nCAPTURE=%s\nSTDIO_BUFFER=0\n",
                              (long)getpid(),path);
            if (size<0 || size>=(int)sizeof(text)) _exit(125);
            int fd=open(proof,O_WRONLY|O_CREAT|O_EXCL,0600);
            if (fd<0 || write(fd,text,size)!=size || close(fd)) _exit(125);
        }
        static const char notice[]="RetroPlayer observation: unbuffered WAV capture\n";
        if(write(STDERR_FILENO,notice,sizeof(notice)-1)<0){}
    }
    return stream;
}
FILE *fopen(const char *path,const char *mode)
{ return open_capture(path,mode,"fopen"); }
FILE *fopen64(const char *path,const char *mode)
{ return open_capture(path,mode,"fopen64"); }
