#ifndef _CTYPE_H
#define _CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif

int toupper(int c);
int tolower(int c);
int isdigit(int c);
int isalpha(int c);
int isspace(int c);
int isprint(int c);

#ifdef __cplusplus
}
#endif

#endif
