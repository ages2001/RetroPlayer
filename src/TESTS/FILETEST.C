/* Exercise VLC file descriptors with the same CRT as its stdio callers. */
#include "config.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_block.h>
static FILE *report;
static unsigned checks,failures;
#define CHECK(e) do { ++checks;if(!(e)) { ++failures;fprintf(report,"FAIL LINE=%u ERRNO=%d\r\n",__LINE__,errno);fflush(report); } } while(0)
#define APIS(X) X(vlc_open) X(vlc_fopen) X(vlc_rename) X(vlc_unlink) X(block_FilePath)
#define DECLARE(n) static __typeof__(&n) rp_##n;
APIS(DECLARE)
int main(void)
{
    char folder[MAX_PATH],*last,buf[32];DWORD n=GetModuleFileNameA(NULL,folder,MAX_PATH);
    if(!n||n>=MAX_PATH-24)return 2;
    last=folder;for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;*last=0;
    SetCurrentDirectoryA(folder);
    report=fopen("FILETEST.TXT","wb");if(!report)return 3;
    HMODULE core=LoadLibraryA("LIBCORE.DLL");CHECK(core!=NULL);if(!core)goto done;
#define BIND(n) rp_##n=(__typeof__(&n))GetProcAddress(core,#n);CHECK(rp_##n!=NULL);
    APIS(BIND)
    if(failures)goto done;
    DeleteFileA("IOCHECK.BIN");DeleteFileA("IOMOVED.BIN");
    FILE *f=rp_vlc_fopen("IOCHECK.BIN","w+b");CHECK(f!=NULL);
    if(f) {
        CHECK(fwrite("one\0two",1,7,f)==7);CHECK(fflush(f)==0);
        CHECK(fseek(f,0,SEEK_SET)==0);CHECK(fread(buf,1,7,f)==7);
        CHECK(memcmp(buf,"one\0two",7)==0);CHECK(fclose(f)==0);
    }
    for(unsigned writable=0;writable<2;++writable) {
        block_t *block=rp_block_FilePath("IOCHECK.BIN",writable!=0);CHECK(block!=NULL);
        if(block) {
            CHECK(block->i_buffer==7 && memcmp(block->p_buffer,"one\0two",7)==0);
            if(writable)block->p_buffer[0]='X';
            block_Release(block);
        }
    }
    int fd=rp_vlc_open("IOCHECK.BIN",O_WRONLY|O_CREAT|O_APPEND|O_BINARY,0666);
    CHECK(fd>=0);if(fd>=0) { CHECK(_write(fd,"!",1)==1);CHECK(_close(fd)==0); }
    f=rp_vlc_fopen("IOCHECK.BIN","rb");CHECK(f!=NULL);
    if(f) { CHECK(fread(buf,1,sizeof(buf),f)==8);CHECK(memcmp(buf,"one\0two!",8)==0);CHECK(fclose(f)==0); }
    errno=0;fd=rp_vlc_open("IOCHECK.BIN",O_CREAT|O_EXCL|O_TRUNC|O_WRONLY,0666);
    CHECK(fd==-1 && errno==EEXIST);if(fd>=0)_close(fd);
    f=rp_vlc_fopen("IOMOVED.BIN","wb");CHECK(f!=NULL);if(f)CHECK(fclose(f)==0);
    CHECK(rp_vlc_rename("IOCHECK.BIN","IOMOVED.BIN")==0);
    f=rp_vlc_fopen("IOMOVED.BIN","rb");CHECK(f!=NULL);
    if(f) { CHECK(fread(buf,1,sizeof(buf),f)==8);CHECK(memcmp(buf,"one\0two!",8)==0);CHECK(fclose(f)==0); }
    errno=0;CHECK(rp_vlc_rename("IOCHECK.BIN","IOMOVED.BIN")==-1 && errno==ENOENT);
    fd=rp_vlc_open("IOMOVED.BIN",O_WRONLY|O_TRUNC|O_BINARY);CHECK(fd>=0);
    if(fd>=0)CHECK(_close(fd)==0);
    block_t *empty=rp_block_FilePath("IOMOVED.BIN",false);CHECK(empty!=NULL);
    if(empty) { CHECK(empty->i_buffer==0);block_Release(empty); }
    f=rp_vlc_fopen("IOMOVED.BIN","rb");CHECK(f!=NULL);
    if(f) { CHECK(fread(buf,1,1,f)==0 && feof(f));CHECK(fclose(f)==0); }
    f=rp_vlc_fopen("IOMOVED.BIN","wt");CHECK(f!=NULL);
    if(f) { CHECK(fwrite("a\nb",1,3,f)==3);CHECK(fclose(f)==0); }
    f=rp_vlc_fopen("IOMOVED.BIN","rb");CHECK(f!=NULL);
    if(f) { CHECK(fread(buf,1,sizeof(buf),f)==4);CHECK(memcmp(buf,"a\r\nb",4)==0);CHECK(fclose(f)==0); }
    CHECK(rp_vlc_unlink("IOMOVED.BIN")==0);
done:
    if(core)FreeLibrary(core);
    fprintf(report,"CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",checks,failures,failures?"FAIL":"PASS");
    fclose(report);return failures?1:0;
}
