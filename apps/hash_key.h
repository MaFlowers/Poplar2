#ifndef __HASH_KEY_H__
#define __HASH_KEY_H__

/*
	��ȡ�ַ�����ϣֵ�ļ�������
*/

unsigned int SDBMHash(char *str);
unsigned int RSHash(char *str);
unsigned int JSHash(char *str);
unsigned int PJWHash(char *str);
unsigned int ELFHash(char *str);
unsigned int DJBHash(char *str);
unsigned int APHash(char *str);
unsigned int BKDRHash(char *str);

#endif
