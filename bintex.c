/*  Copyright 2010-2018, JP Norair
  *
  * Licensed under the OpenTag License, Version 1.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * http://www.indigresso.com/wiki/doku.php?id=opentag:license_1_0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */

/**
  * @file       bintex.c
  * @author     JP Norair
  * @version    V1.1
  * @date       27 Jun 2018
  * @brief      BinTex parser
  * @ingroup    BinTex
  *
  ******************************************************************************
  */

#include "bintex.h"
#include <string.h>


#define IS_WHITESPACE(VAL)  ((VAL==' ')||(VAL=='\r')||(VAL=='\n')||(VAL=='\t'))
#define IS_HEXVAL(VAL)      ((((VAL)>='0') && ((VAL)<='9')) || (((VAL)>='a') && ((VAL)<='f')) || (((VAL)>='A') && ((VAL)<='F')))
#define IS_DECVAL(VAL)      (((VAL)>='0') && ((VAL)<='9'))
#define IS_BINVAL(VAL)      (((VAL)>='0') && ((VAL)<='1'))


typedef enum {
    DATA_EOF = 0,
    DATA_error,
    DATA_lineterm,
    DATA_comment,
    DATA_ascii,
    DATA_binnum,
    DATA_hexnum,
    DATA_hexblock,
    DATA_decnum,
    DATA_decblock
} Data_type;


/// Global Variables.  
///@todo Should be moved into object.
static int (*sub_getc)(void* stream);
static int (*sub_validatehex)(void* stream);
static int (*sub_validatedec)(void* stream);


static int sub_buffergetc(void* stream);
static int sub_filegetc(void* stream);
static int sub_buffer_validatehex(void* stream);
static int sub_file_validatehex(void* stream);
static int sub_buffer_validatedec(void* stream);
static int sub_file_validatedec(void* stream);


static int sub_parsestream(void* stream, bintex_q* msg);
static Data_type sub_parse_header(void* stream);
static int sub_passcomment(void* stream);
static int sub_getascii(void* stream, bintex_q* msg);
static int sub_gethexblock(void* stream, bintex_q* msg);
static int sub_getdecblock(void* stream, bintex_q* msg);
static int sub_gethexnum(int* status, void* stream, bintex_q* msg);
static int sub_getbinnum(int* status, void* stream, bintex_q* msg);
static char sub_char2hex(char input);
static int sub_getdecnum(int* status, void* stream, bintex_q* msg);

static int sub_bindigits(int* status, void* stream, char* buf, int limit);
static int sub_hexdigits(int* status, void* stream, char* buf, int limit);
static int sub_decdigits(int* status, void* stream, char* buf, int limit);


static void q_init(bintex_q* q, uint8_t* buffer, uint16_t alloc);
static void q_rebase(bintex_q *q, uint8_t* buffer);
static void q_copy(bintex_q* q1, bintex_q* q2);
static int16_t q_length(bintex_q* q);
static int16_t q_span(bintex_q* q);
static int16_t q_space(bintex_q* q);
static void q_empty(bintex_q* q);
static int16_t q_length(bintex_q* q);
static int16_t q_span(bintex_q* q);
static int16_t q_space(bintex_q* q);
static uint8_t* q_start(bintex_q* q, int offset, uint16_t options);
static uint8_t* q_markbyte(bintex_q* q, int shift);
static void q_writebyte(bintex_q* q, uint8_t byte_in);
static void q_writeshort(bintex_q* q, uint16_t short_in);
static void q_writeshort_be(bintex_q* q, uint16_t short_in);
static void q_writelong(bintex_q* q, uint32_t long_in);
static uint8_t q_readbyte(bintex_q* q);
static uint16_t q_readshort(bintex_q* q);
static uint16_t q_readshort_be(bintex_q* q);
static uint32_t q_readlong(bintex_q* q);
static void q_writestring(bintex_q* q, uint8_t* string, int length);
static void q_readstring(bintex_q* q, uint8_t* string, int length);




typedef union {
    uint16_t    ushort;
    int16_t     sshort;
    uint8_t     ubyte[2];
    int8_t      sbyte[2];
} ot_uni16;

typedef union {
    uint32_t    ulong;
    int32_t     slong;
    uint16_t    ushort[2];
    int16_t     sshort;
    uint8_t     ubyte[4];
    int8_t      sbyte[4];
} ot_uni32;



int bintex_iter_fq(FILE* file, bintex_q* msg) {
    sub_getc        = &sub_filegetc;
    sub_validatehex = &sub_file_validatehex;
    sub_validatedec = &sub_file_validatedec;
    
    return sub_parsestream((void*)file, msg);
}

int bintex_fs(FILE* file, unsigned char* stream_out, int size) {
    bintex_q local;
    q_init(&local, stream_out, size);
    
    while (1) {
        int test;
        
        test = bintex_iter_fq(file, &local);
        if (test < 0) break;
        
#       ifdef __DEBUG__
        {
            int i;
            uint8_t* s;
            fprintf(stdout, "Data written to queue\n");
            
            for (s=(local.putcursor-test), i=0; s<local.putcursor; s++, i++) {
                if ((i & 3) == 0) {
                    fprintf(stdout, "%04X: ", (uint16_t)s);
                }
                fprintf(stdout, "%02X ", *s);
                if ((i & 3) == 3) {
                    fputc('\n', stdout);
                }
            } 
    
            if ((i & 3) != 0) {
                fputc('\n', stdout);
            }
            
            fputc('\n', stdout);
        }
#       endif
    }
    
    return (local.putcursor - local.front);
}






int bintex_iter_sq(unsigned char **string, bintex_q* msg, int size) {
    sub_getc        = &sub_buffergetc;
    sub_validatehex = &sub_buffer_validatehex;
    sub_validatedec = &sub_buffer_validatedec;
    
    return sub_parsestream((void*)string, msg);
}

int bintex_ss(unsigned char *string, unsigned char* stream_out, int size) {
    bintex_q local;
    
    q_init(&local, stream_out, size);
    
    while (1) {
        int test;
        
        test    = bintex_iter_sq(&string, &local, size);
        size   -= test;
        if (test < 0) break;
        
#       ifdef __DEBUG__
        {
            int i;
            uint8_t* s;
            fprintf(stdout, "Data written to queue\n");
            
            for (s=(local.putcursor-test), i=0; s<local.putcursor; s++, i++) {
                if ((i & 3) == 0) {
                    fprintf(stdout, "%04X: ", (uint16_t)s);
                }
                fprintf(stdout, "%02X ", *s);
                if ((i & 3) == 3) {
                    fputc('\n', stdout);
                }
            } 
    
            if ((i & 3) != 0) {
                fputc('\n', stdout);
            }
            
            fputc('\n', stdout);
        }
#       endif
    }
    
    return (local.putcursor - local.front);
}




// Input Parser Tester (comment out when using library)
#ifdef __BINTEX_TEST__
int parsefile_main(int argc, char** argv);
int parsestring_main(int argc, char** argv);

int main(int argc, char** argv) {
    //return parsefile_main(argc, argv);
    return parsestring_main(argc, argv);
}


int parsefile_main(int argc, char** argv) {
    FILE*   fp;
    bintex_q    msg;
    uint8_t     msg_buffer[512];
    int         usage_type = 0;
    
    // File is specified
    if (argc > 1) {
        fp = fopen(argv[1], "r");
        
        if (fp != NULL) {
            usage_type = 1;
        }
        else {
            usage_type = 0;
            fprintf(stderr, "Error, could not open file: %s\n", argv[1]);
        }
    }
    
    if (usage_type == 0) {
        fprintf(stderr, "Usage: app [filename]\n");
        return 0;
    }

    printf("%d Bytes written to output\n", bintex_fs(fp, msg_buffer, 512));
    
    return 0;
}



int parsestring_main(int argc, char** argv) {
    unsigned char   string[512];
    unsigned char   stream[512];
    unsigned char*  input;
    unsigned char*  output;
    size_t bytes_out;
    
    //strcpy((char*)string, "[00 11 22 33] (32 64 96 128) d-5930 x9933 \"Blah\"");
    strcpy((char*)string, "[11223344 55667788 2233445566]");
    
    input   = string;
    
    bytes_out = bintex_ss(input, stream, 512);
    
    printf("%zu Bytes written to output\n", bytes_out);
    
    for (int i=0; i<bytes_out; i++) {
        printf("%02X ", stream[i]);
    }
    printf("\n");
    
    return 0;
}
    
#endif







static int sub_buffergetc(void* stream) {
    unsigned char c;
    unsigned char **s;
    s   = (unsigned char**)stream;
    c   = **s;          //get character
    *s  = *s + 1;       //increment buffer
    
    if (c != 0) {     //no EOFs
        return c;
    }
    else {
        return -1;
    }    
}


static int sub_filegetc(void* stream) {
    return fgetc((FILE*)stream);
}


static int sub_buffer_validatehex(void* stream) {
    char* front;
    int bytes_read;
    
    front = *(char**)stream;
    
    while (1) {
        char a = *front++;
        bytes_read++;
        
        if (a == 0) {
            break;
        }
        if (a == ']') {
            bytes_read = 0;
            break;
        }
        if (!IS_HEXVAL(a) && !IS_WHITESPACE(a)) {
            break;
        }
    }
    
    return bytes_read;
}

static int sub_file_validatehex(void* stream) {
    fpos_t pos;
    int bytes_read;
    
    fgetpos((FILE*)stream, &pos);
    
    while (1) {
        int a = fgetc((FILE*)stream);
        bytes_read++;
        
        if (a == EOF) {
            break;
        }
        if (a == ']') {
            bytes_read = 0;
            break;
        }
        if (!IS_HEXVAL(a) && !IS_WHITESPACE(a)) {
            break;
        }
    }
    
    fsetpos((FILE*)stream, &pos);
    
    return bytes_read;
}

static int sub_buffer_validatedec(void* stream) {
    char* front;
    int bytes_read;
    
    front = *(char**)stream;
    
    while (1) {
        char a = *front++;
        bytes_read++;
        
        if (a == 0) {
            break;
        }
        if (a == ')') {
            bytes_read = 0;
            break;
        }
        if (!IS_DECVAL(a) && !IS_WHITESPACE(a)) {
            break;
        }
    }
    
    return bytes_read;
}

static int sub_file_validatedec(void* stream) {
    fpos_t pos;
    int bytes_read;
    
    fgetpos((FILE*)stream, &pos);
    
    while (1) {
        int a = fgetc((FILE*)stream);
        bytes_read++;
        
        if (a == EOF) {
            break;
        }
        if (a == ')') {
            bytes_read = 0;
            break;
        }
        if (!IS_DECVAL(a) && !IS_WHITESPACE(a)) {
            break;
        }
    }
    
    fsetpos((FILE*)stream, &pos);
    
    return bytes_read;
}





static int sub_parsestream(void* stream, bintex_q* msg) {
    int status;

    switch (sub_parse_header(stream)) {
        case DATA_EOF:      return -1;
        case DATA_error:    return -2;
        case DATA_lineterm: return -3;
        case DATA_comment:  return sub_passcomment(stream);
        case DATA_ascii:    return sub_getascii(stream, msg);
        case DATA_binnum:   return sub_getbinnum(&status, stream, msg);
        case DATA_hexnum:   return sub_gethexnum(&status, stream, msg);
        case DATA_hexblock: return sub_gethexblock(stream, msg);
        case DATA_decnum:   return sub_getdecnum(&status, stream, msg);
        case DATA_decblock: return sub_getdecblock(stream, msg);
    }
    
    return -2;
}







static Data_type sub_parse_header(void* stream) {
    char next;

    parse_header_getchar:
    next = sub_getc(stream);
    switch (next) {
        case '\n':  //Bypass Newlines
        case '\r':  //Bypass returns
        case '\t':  //Bypass tabs
        case '0':   //Bypass leading 0's (in case person uses "0x")
        case ' ':   //Bypass space
                    goto parse_header_getchar;
        
        case '#':   return DATA_comment;
        case '"':   return DATA_ascii;
        case 'b':   return DATA_binnum;
        case 'x':   return DATA_hexnum;
        case '[':   return DATA_hexblock;
        case 'd':   return DATA_decnum;
        case '(':   return DATA_decblock;
        
        case ';':   return DATA_lineterm;
        case -1:    return DATA_EOF;
        
        default:    return DATA_error;
    }
}



static int sub_passcomment(void* stream) {
    char subcomment[8];
    int next;
    int i = 0;
    //FILE* outfp = NULL;
    //int action = 0;
    
    //buffer subcomment
    while (i<8) {
        next            = sub_getc(stream);
        subcomment[i++] = next;
        
        switch (next) {
            case -1:    return -1;
            case '\n':  return 0;
            case '\r':
            case '\t':
            case ' ':   goto sub_passcomment_passws;
            
            //check subcomment (this could grow in the future)
            //case '>':   outfp   = stdout;
            //            action  = 1;
            //            goto sub_passcomment_passws;
        }
    }
    
    //bypass whitespace after subcomment
    sub_passcomment_passws:
    next = sub_getc(stream);
    switch (next) {
        case -1:    return -1;
        case '\n':  return 0;
        case '\r':
        case '\t':
        case ' ':   goto sub_passcomment_passws;
    }
    
    // do something with the comment, if action requires
    while (next >= 0) {
        //switch (action) {
        //    case 1: fputc(next, outfp); 
        //            break;
        //}
        if (next == '\n') 
            return 0;
        
        next = sub_getc(stream);
    } 
    
    return -1;
}





static int sub_getascii(void* stream, bintex_q* msg) {
    char    next;
    int     bytes_written;
    
    bytes_written = q_length(msg);
    
    while (1) {
        next = sub_getc(stream);
        
        if (next == '"') {
            break;   
        }
        
        if (next == '\\') {
            switch (sub_getc(stream)) {
                case 'a':   next = '\a';    break;
                case '\\':  next = '\\';    break;
                case 'b':   next = '\b';    break;
                case 'r':   next = '\r';    break;
                case '"':   next = '\"';    break;
                case 'f':   next = '\f';    break;
                case 't':   next = '\t';    break;
                case 'n':   next = '\n';    break;
                case '0':   next = '\0';    break;
                case '\'':  next = '\'';    break;
                case 'v':   next = '\v';    break;
                case '?':   next = '\?';    break;
            } 
        }
        
        q_writebyte(msg, next);
    }
    
    bytes_written = (q_length(msg) - bytes_written);
    return bytes_written;
}




static int sub_gethexblock(void* stream, bintex_q* msg) {
    int status;
    int bytes_written;
    bytes_written = q_length(msg);

    // Validate the hex block
    status = sub_validatehex(stream);

    while (status == 0) {
        sub_gethexnum(&status, stream, msg);
    }

    bytes_written = q_length(msg) - bytes_written;
    return bytes_written;
}




static int sub_getdecblock(void* stream, bintex_q* msg) {
    int status = 0;
    int bytes_written = q_length(msg);



    while (status == 0) {
        sub_getdecnum(&status, stream, msg);
    }

    bytes_written = q_length(msg) - bytes_written;
    return bytes_written;
}



static int sub_getbinnum(int* status, void* stream, bintex_q* msg) {
    int     digits;
    int     i = 0;
    int     shift;
    char    next;
    char    byte = 0;
    char    buf[33];
    
    digits = sub_bindigits(status, stream, buf, 32);
    
    // If the length of digits is not byte-aligned, pad first byte
    shift = (digits & 7);
    
    do {
        while (shift > 0) {
            shift--;
            next    = buf[i++];
            next   &= 1;    // '0' = 48, '1' = 49, so just take lsb
            byte   |= next << shift;
        }
        if (i != 0) {
            q_writebyte(msg, byte);
        }
        shift   = 8;
        byte    = 0;
    } 
    while (i < digits);
    
    return (i+7)/8;
}



static int sub_gethexnum(int* status, void* stream, bintex_q* msg) {
    int     digits;
    int     i = 0;
    char    buf[72];

    digits = sub_hexdigits(status, stream, buf, 64);
    
    // If the length of digits is odd, write the first hex nibble as a byte
    if (digits & 1) {      
        q_writebyte(msg, sub_char2hex(buf[i++]));
    }
    
    while (i < digits) {       
        char byte_data;
        byte_data = (sub_char2hex(buf[i++]) << 4) & 0xF0;
        byte_data |= sub_char2hex(buf[i++]) & 0x0F;
        q_writebyte(msg, byte_data);        
    }
    
    return (i+1)/2;
}




static char sub_char2hex(char input) {
    char output = input;

    if ((input >= '0') && (input <= '9')) {
        output -= '0';
    }
    else if ((input >= 'a') && (input <= 'f')) {
        output -= ('a' - 10);
    }
    else if ((input >= 'A') && (input <= 'F')) {
        output -= ('A' - 10);
    }
    else {
        output = 0;
    }
    
    return output;
}





static int sub_getdecnum(int* status, void* stream, bintex_q* msg) {
    int     digits;
  //char    next;
    char    buf[16];
    int     sign    = 1;
    int     force_u = 0;
    int     number  = 0;
    int     i       = 0;
    int     size    = 0;
    
    // Buffer until whitespace or ')' delimiter 
    digits = sub_decdigits(status, stream, buf, 15);
    
    // Deal with leading minus sign
    if (buf[i] == '-') {
        i++;
        sign = -1;
    }
    
    // Go through the digits & footer
    // - load in numerical value, one digit at a time
    // - also look for the type footer: ul, us, uc, u, l, s, c, or none
    while (i < digits) {
        if ((buf[i] >= '0') && (buf[i] <= '9')) {
            number *= 10;
            number += (buf[i++] - '0');
        }
        else {
            force_u = (buf[i] == 'u');
            i      += force_u;

            if (buf[i] == 'c')      size = 1;   // c: char (1 byte)
            else if (buf[i] == 's') size = 2;   // s: short (2 bytes)
            else if (buf[i] == 'l') size = 3;   // l: long (4 bytes)
            break;
        }
    }
    
    // Determine size in case where footer is not explicitly provided
    if (size == 0) {
        int j;
        int bound[] = {128, 256, 32768, 65536, 0, 0};
        int max     = number - (sign < 0);
        
        for (j=force_u, size=1;  ; j+=2, size++) {
            if ((bound[j]==0) || (bound[j]>=max)) break;
        }
    }

    number *= sign;

    switch (size & 3) {
        case 0:
        case 1: q_writebyte(msg, (uint8_t)number);
                break;
        
        case 2: q_writeshort(msg, (uint16_t)number);
                break;
        
        case 3: size = 4;
                q_writelong(msg, (uint32_t)number);
                break;
    }
    
    return size;
}


static int sub_hexdigits(int* status, void* stream, char* buf, int limit) {
    int digits;
    digits  = 0;
    *status = 0;
        
    while (digits < limit) {
        buf[digits] = sub_getc(stream);
        if (buf[digits] == ']') {
            *status = 1;
            break;
        }
        if (!IS_HEXVAL(buf[digits])) {
            if (!IS_WHITESPACE(buf[digits])) {
                *status = 2;
            }
            break;
        }
        digits++;
    }
    
    return digits;
}

static int sub_decdigits(int* status, void* stream, char* buf, int limit) {
    int digits;
    digits  = 0;
    *status = 0;
        
    while (digits < limit) {
        buf[digits] = sub_getc(stream);
        if (buf[digits] == ')') {
            *status = 1;
            break;
        }
        if (!IS_DECVAL(buf[digits])) {
            if (!IS_WHITESPACE(buf[digits])) {
                *status = 2;
            }
            break;
        }
        digits++;
    }
    
    return digits;
}

static int sub_bindigits(int* status, void* stream, char* buf, int limit) {
    int digits;
    digits  = 0;
    *status = 0;
        
    while (digits < limit) {
        buf[digits] = sub_getc(stream);
        if (!IS_BINVAL(buf[digits])) {
            if (!IS_WHITESPACE(buf[digits])) {
                *status = 2;
            }
            break;
        }
        digits++;
    }
    
    return digits;
}





/** Internal Queue Module Implementation.
    Could be broken into separate files
 */

static void q_init(bintex_q* q, uint8_t* buffer, uint16_t alloc) {
    q->alloc    = alloc;
    q->front    = buffer;
    q->back     = buffer+alloc;
    q_empty(q);
}



static void q_rebase(bintex_q *q, uint8_t* buffer) {
    q->front        = buffer;
    q->getcursor    = buffer;
    q->putcursor    = buffer;
    q->back         = buffer;
}


static void q_copy(bintex_q* q1, bintex_q* q2) {
    memcpy((uint8_t*)q1, (uint8_t*)q2, sizeof(bintex_q));
}


static int16_t q_length(bintex_q* q) {
    return (q->putcursor - q->front);
}

static int16_t q_span(bintex_q* q) {
    return (q->putcursor - q->getcursor);
}

static int16_t q_space(bintex_q* q) {
    return (q->back - q->putcursor);
}




static void q_empty(bintex_q* q) {
    //#q->length           = 0;
    q->options          = 0;
    q->back             = q->front + q->alloc;
    q->putcursor        = q->front;
    q->getcursor        = q->front;
}



static uint8_t* q_start(bintex_q* q, int offset, uint16_t options) {  
    q_empty(q);

    if (offset >= q->alloc) 
        return NULL;  
    
    q->options         = options;
    //#q->length          = offset;
    q->putcursor      += offset;
    q->getcursor      += offset;
    return q->getcursor;
}



static uint8_t* q_markbyte(bintex_q* q, int shift) {
    uint8_t* output;
    output          = q->getcursor;
    q->getcursor   += shift;
    return output;
}



static void q_writebyte(bintex_q* q, uint8_t byte_in) {
    *q->putcursor++ = byte_in;
    //#q->length++;
}



static void q_writeshort(bintex_q* q, uint16_t short_in) {
    uint8_t* data;
    data = (uint8_t*)&short_in;

#   ifdef __BIG_ENDIAN__
        *q->putcursor++ = data[0];
        *q->putcursor++ = data[1];
#   else
        *q->putcursor++ = data[1];
        *q->putcursor++ = data[0];
#   endif
    
    //#q->length     += 2;
}



static void q_writeshort_be(bintex_q* q, uint16_t short_in) {
#   ifdef __BIG_ENDIAN__
        q_writeshort(q, short_in);

#   else
        uint8_t* data;
        data            = (uint8_t*)&short_in;
        *q->putcursor++ = data[0];
        *q->putcursor++ = data[1];
        
        //#q->length     += 2;
#   endif    
}



static void q_writelong(bintex_q* q, uint32_t long_in) {
    uint8_t* data;
    data = (uint8_t*)&long_in;

#   ifdef __BIG_ENDIAN__
        *q->putcursor++ = data[0];
        *q->putcursor++ = data[1];
        *q->putcursor++ = data[2];
        *q->putcursor++ = data[3];
#   else
        *q->putcursor++ = data[3];
        *q->putcursor++ = data[2];
        *q->putcursor++ = data[1];
        *q->putcursor++ = data[0];
#   endif
    
    //q->putcursor  += 4;
    //#q->length     += 4;
}



static uint8_t q_readbyte(bintex_q* q) {
    return *(q->getcursor++);
}



static uint16_t q_readshort(bintex_q* q) {
    ot_uni16 data;

#   ifdef __BIG_ENDIAN__
        data.ubyte[0]   = *q->getcursor++;
        data.ubyte[1]   = *q->getcursor++;
#   else
        data.ubyte[1]   = *q->getcursor++;
        data.ubyte[0]   = *q->getcursor++;
#   endif
    
    //q->getcursor  += 2;
    return data.ushort;
}



static uint16_t q_readshort_be(bintex_q* q) {
#   ifdef __BIG_ENDIAN__
        return q_readshort(q);
#   else
        ot_uni16 data;
        data.ubyte[0]   = *q->getcursor++;
        data.ubyte[1]   = *q->getcursor++;

        return data.ushort;
#   endif
}


static uint32_t q_readlong(bintex_q* q)  {
    ot_uni32 data;

#   ifdef __BIG_ENDIAN__
        data.ubyte[0]   = *q->getcursor++;
        data.ubyte[1]   = *q->getcursor++;
        data.ubyte[2]   = *q->getcursor++;
        data.ubyte[3]   = *q->getcursor++;
#   else
        data.ubyte[3]   = *q->getcursor++;
        data.ubyte[2]   = *q->getcursor++;
        data.ubyte[1]   = *q->getcursor++;
        data.ubyte[0]   = *q->getcursor++;
#   endif
    
    return data.ulong;
}


static void q_writestring(bintex_q* q, uint8_t* string, int length) {
    memcpy(q->putcursor, string, length);
    //#q->length      += length;
    q->putcursor   += length;
}


static void q_readstring(bintex_q* q, uint8_t* string, int length) {
    memcpy(string, q->getcursor, length);
    q->getcursor += length;
}



#if (defined(__STDC__) || defined (__POSIX__))
#include <stdio.h>

static void q_print(bintex_q* q) {
    int length;
    int i;
    int row;
    length = q_length(q);
    
    printf("Queue Length/Alloc: %d/%d\n", length, q->alloc);
    printf("Queue Getcursor:    %d\n", (int)(q->getcursor-q->front));
    printf("Queue Putcursor:    %d\n", (int)(q->putcursor-q->front));
    
    for (i=0, row=0; length>0; ) {
        length -= 8;
        row    += (length>0) ? 8 : 8+length;
        printf("%04X: ", i);
        for (; i<row; i++) {
            printf("%02X ", q->front[i]);
        }
        printf("\n");
    }
    printf("\n");
}

#endif

