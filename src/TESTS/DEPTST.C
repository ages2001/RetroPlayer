/* Exercise real compressed data, XML parsing, bidi, shaping and rasterization. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <io.h>
#include <iconv.h>
#include <zlib.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <fribidi.h>
static unsigned checks, failures;
static FILE *report;
static LONG WINAPI exception_report(EXCEPTION_POINTERS *e)
{
    fprintf(report,"EXCEPTION=%08lx ADDRESS=%08lx IMAGE=%08lx\r\nRESULT=FAIL\r\n",
        e->ExceptionRecord->ExceptionCode,(DWORD)e->ExceptionRecord->ExceptionAddress,
        (DWORD)GetModuleHandleA(NULL));fflush(report);
    return EXCEPTION_EXECUTE_HANDLER;
}
#define STEP(s) do { fputs("STEP=" s "\r\n",report);fflush(report); } while(0)
extern int __cdecl __gthr_win32_create(void **,void *(*)(void *),void *);
extern int __cdecl __gthr_win32_join(void *,void **);
static void *xml_worker(void *arg)
{
    (void)arg;
    for(unsigned i=0;i<100;++i) {
        xmlDocPtr doc=xmlReadMemory("<root>ok</root>",15,"THREAD.XML",NULL,XML_PARSE_NONET);
        if(!doc) return (void *)1;
        xmlFreeDoc(doc);
        doc=xmlReadMemory("<broken>",8,"BAD.XML",NULL,XML_PARSE_NONET|XML_PARSE_NOERROR|XML_PARSE_NOWARNING);
        if(doc) { xmlFreeDoc(doc);return (void *)2; }
    }
    return NULL;
}
#define CHECK(e) do { ++checks; if(!(e)) { ++failures; fprintf(report,"FAIL LINE=%u\r\n",__LINE__); fflush(report); } } while(0)
static void conversion(const char *from,const char *to,const char *input,size_t size,
                       const char *expected,size_t expected_size)
{
    iconv_t cd=iconv_open(to,from);char output[128],*out=output;
    char *in=(char *)input;size_t left=size,space=sizeof(output);
    CHECK(cd!=(iconv_t)-1);if(cd==(iconv_t)-1)return;
    CHECK(iconv(cd,&in,&left,&out,&space)==0);
    CHECK(left==0 && (size_t)(out-output)==expected_size);
    CHECK(memcmp(output,expected,expected_size)==0);
    CHECK(iconv_close(cd)==0);
}
static void character_tests(void)
{
    STEP("ICONV");
    /* Embedded NULs, a supplementary character, and explicit endianness. */
    conversion("UTF-8","UTF-32LE","A\0\xc4\x9f\xf0\x9f\x98\x80",8,
               "A\0\0\0\0\0\0\0\x1f\x01\0\0\0\xf6\x01\0",16);
    conversion("UTF-16LE","UTF-8","\x3d\xd8\0\xde",4,"\xf0\x9f\x98\x80",4);
    conversion("CP1254","UTF-8","I\xf0\x64\xfd\x72",5,"I\xc4\x9f\x64\xc4\xb1\x72",7);
    conversion("SHIFT_JIS","UTF-8","\x93\xfa\x96\x7b",4,"\xe6\x97\xa5\xe6\x9c\xac",6);
    conversion("ISO-2022-JP","UTF-8","\x1b$BF|K\\\x1b(B",10,"\xe6\x97\xa5\xe6\x9c\xac",6);
    iconv_t cd=iconv_open("UTF-32LE","UTF-8");
    CHECK(cd!=(iconv_t)-1);if(cd==(iconv_t)-1)return;
    char output[16],*out=output,*in="\xc0\xaf";size_t left=2,space=16;
    errno=0;CHECK(iconv(cd,&in,&left,&out,&space)==(size_t)-1 && errno==EILSEQ);
    CHECK(left==2 && out==output && space==16);
    in="\xe2\x82";left=2;errno=0;
    CHECK(iconv(cd,&in,&left,&out,&space)==(size_t)-1 && errno==EINVAL);
    CHECK(left==2 && out==output);
    in="A";left=1;space=3;errno=0;
    CHECK(iconv(cd,&in,&left,&out,&space)==(size_t)-1 && errno==E2BIG);
    CHECK(left==1 && out==output && space==3);
    space=16;CHECK(iconv(cd,&in,&left,&out,&space)==0 && left==0 && out==output+4);
    CHECK(iconv(cd,NULL,NULL,NULL,NULL)==0);CHECK(iconv_close(cd)==0);
    errno=0;CHECK(iconv_open("NOT_AN_ENCODING","UTF-8")== (iconv_t)-1 && errno==EINVAL);
}
int main(void)
{
    char folder[MAX_PATH],*last,*p;
    unsigned char input[8192],compressed[10000],output[8192];
    uLongf clen=sizeof(compressed),olen=sizeof(output);
    unsigned i;
    GetModuleFileNameA(NULL,folder,MAX_PATH);last=folder;
    for(p=folder;*p;++p) if(*p=='\\') last=p+1;
    *last=0;SetCurrentDirectoryA(folder);
    report=fopen("DEPTST.TXT","wb");if(!report) return 2;
    SetUnhandledExceptionFilter(exception_report);
    fputs("Portable dependency integration test\r\n",report);fflush(report);
    character_tests();
    STEP("ZLIB");
    for(i=0;i<sizeof(input);++i) input[i]=(unsigned char)(i*37+(i>>5));
    CHECK(compress2(compressed,&clen,input,sizeof(input),9)==Z_OK);
    CHECK(uncompress(output,&olen,compressed,clen)==Z_OK && olen==sizeof(input));
    CHECK(memcmp(input,output,sizeof(input))==0);
    CHECK(crc32(0,(const Bytef *)"123456789",9)==0xcbf43926UL);
    compressed[0]^=0xff;olen=sizeof(output);
    CHECK(uncompress(output,&olen,compressed,clen)==Z_DATA_ERROR);
    gzFile zip=gzopen_w(L"PACK.GZ", "wb");CHECK(zip!=NULL);
    if(zip) { CHECK(gzwrite(zip,input,sizeof(input))==sizeof(input));CHECK(gzclose(zip)==Z_OK); }
    zip=gzopen_w(L"PACK.GZ", "rb");CHECK(zip!=NULL);
    if(zip) { CHECK(gzread(zip,output,sizeof(output))==sizeof(output));CHECK(memcmp(input,output,sizeof(input))==0);CHECK(gzclose(zip)==Z_OK); }
    CHECK(DeleteFileA("PACK.GZ"));
    const char xml[]="<playlist><item>\xc3\xa9</item><item>two</item></playlist>";
    STEP("XML");
    xmlDocPtr doc=xmlReadMemory(xml,sizeof(xml)-1,"TEST.XML",NULL,XML_PARSE_NONET);
    CHECK(doc!=NULL);
    if(doc) {
        xmlNodePtr root=xmlDocGetRootElement(doc);
        CHECK(root && !xmlStrcmp(root->name,(const xmlChar *)"playlist"));
        xmlXPathContextPtr context=xmlXPathNewContext(doc);CHECK(context!=NULL);
        if(context) {
            xmlXPathObjectPtr result=xmlXPathEvalExpression((const xmlChar *)"count(/playlist/item)",context);
            CHECK(result && result->type==XPATH_NUMBER && result->floatval==2.0);
            if(result) xmlXPathFreeObject(result);
            xmlXPathFreeContext(context);
        }
        xmlChar *content=xmlNodeGetContent(root->children);
        CHECK(content && !xmlStrcmp(content,(const xmlChar *)"\xc3\xa9"));
        xmlFree(content);xmlFreeDoc(doc);
    }
    doc=xmlReadMemory("<a>",3,"BAD.XML",NULL,XML_PARSE_NONET|XML_PARSE_NOERROR|XML_PARSE_NOWARNING);
    CHECK(doc==NULL);if(doc) xmlFreeDoc(doc);
    STEP("XML_THREADS");
    {
        void *threads[8]={0},*result;
        for(i=0;i<8;++i) CHECK(__gthr_win32_create(&threads[i],xml_worker,NULL)==0);
        for(i=0;i<8;++i) if(threads[i]) {
            CHECK(__gthr_win32_join(threads[i],&result)==0);
            CHECK(result==NULL);
        }
    }
    xmlCleanupParser();
    STEP("BIDI");
    FriBidiChar logical[]={0x5d0,0x5d1,0x5d2},visual[3];
    FriBidiParType direction=FRIBIDI_PAR_RTL;
    CHECK(fribidi_log2vis(logical,3,&direction,visual,NULL,NULL,NULL)!=0);
    CHECK(visual[0]==0x5d2 && visual[1]==0x5d1 && visual[2]==0x5d0);
    FT_Library ft=NULL;FT_Face face=NULL;
    STEP("FREETYPE");
    CHECK(FT_Init_FreeType(&ft)==0);
    if(ft) {
        STEP("FONT_UTF8_PATH");
        FILE *source=fopen("DEJAVU.TTF","rb"),*copy=_wfopen(L"F\x00e9.TTF",L"wb");
        int copied=source && copy;char data[4096];size_t got;
        if(copied) {
            while((got=fread(data,1,sizeof(data),source))!=0)
                if(fwrite(data,1,got,copy)!=got) { copied=0;break; }
            if(ferror(source))copied=0;
        }
        if(source && fclose(source))copied=0;
        if(copy && fclose(copy))copied=0;
        CHECK(copied);
        FT_Face unicode=NULL;
        CHECK(FT_New_Face(ft,"F\xc3\xa9.TTF",0,&unicode)==0);
        if(unicode) {
            CHECK(FT_Set_Pixel_Sizes(unicode,0,24)==0);
            CHECK(FT_Load_Char(unicode,0xe9,FT_LOAD_RENDER)==0);
            CHECK(unicode->glyph->bitmap.width>0 && unicode->glyph->bitmap.rows>0);
            FT_Done_Face(unicode);
        }
        CHECK(_wunlink(L"F\x00e9.TTF")==0);
        CHECK(FT_New_Memory_Face(ft,(const FT_Byte *)"not a font",10,0,&face)!=0);
        CHECK(FT_New_Face(ft,"DEJAVU.TTF",0,&face)==0);
        if(face) {
            CHECK(FT_Set_Pixel_Sizes(face,0,24)==0);
            CHECK(FT_Load_Char(face,'A',FT_LOAD_RENDER)==0);
            CHECK(face->glyph->bitmap.width>0 && face->glyph->bitmap.rows>0);
            unsigned pixels=0;
            for(i=0;i<face->glyph->bitmap.rows;++i)
                for(unsigned x=0;x<face->glyph->bitmap.width;++x)
                    pixels+=face->glyph->bitmap.buffer[i*face->glyph->bitmap.pitch+x];
            CHECK(pixels>0);
            hb_font_t *font=hb_ft_font_create_referenced(face);
            STEP("HARFBUZZ");
            hb_buffer_t *buffer=hb_buffer_create();CHECK(hb_buffer_allocation_successful(buffer));
            hb_buffer_add_utf8(buffer,"ffi",3,0,3);hb_buffer_guess_segment_properties(buffer);
            hb_shape(font,buffer,NULL,0);
            CHECK(hb_buffer_get_length(buffer)==1);
            hb_buffer_reset(buffer);
            const char arabic[]="\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85";
            hb_buffer_add_utf8(buffer,arabic,8,0,8);hb_buffer_guess_segment_properties(buffer);
            hb_shape(font,buffer,NULL,0);
            unsigned length;hb_glyph_info_t *glyphs=hb_buffer_get_glyph_infos(buffer,&length);
            CHECK(length==3 && hb_buffer_get_direction(buffer)==HB_DIRECTION_RTL);
            for(i=0;i<length;++i) CHECK(glyphs[i].codepoint!=0);
            hb_buffer_destroy(buffer);hb_font_destroy(font);FT_Done_Face(face);
        }
        FT_Done_FreeType(ft);
    }
    fprintf(report,"CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",checks,failures,failures?"FAIL":"PASS");
    fclose(report);return failures?1:0;
}
