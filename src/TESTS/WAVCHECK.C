/* Confirm that only the selected stream loses its stdio buffer. */
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    FILE *selected=fopen(argv[1],"wb"),*other=fopen(argv[2],"wb");
    if(!selected||!other)return 3;
    struct stat a,b;
    if(fwrite("WAVE",4,1,selected)!=1||fwrite("DATA",4,1,other)!=1)return 4;
    if(fstat(fileno(selected),&a)||fstat(fileno(other),&b))return 5;
    printf("selected=%ld other=%ld\n",(long)a.st_size,(long)b.st_size);
    fclose(selected);fclose(other);
    return 0;
}
