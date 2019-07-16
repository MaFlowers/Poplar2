#include "pop_httpcache.h"
#include "popd.h"
#include "memory.h"
#include "md5.h"
#include <sys/stat.h>
/*
	WEB缓存文件名称生成规则，refers to polipo
	不缓存https网站
*/
static CacheRoot CacheRootT;
static CacheRoot *Root = &CacheRootT;

void CacheRootSetStorePath(char *path)
{
	if(path)
		Root->path = XSTRDUP(MTYPE_HTTP_CACHE_ROOT, path);
	else
		Root->path = XSTRDUP(MTYPE_HTTP_CACHE_ROOT, DEFAULT_HTTP_CACHE_STORE_PATH);

	Root->length = strlen(Root->path);

	fprintf(stdout, "Cache Stroe Path: %s\n", Root->path);
}

void CacheRootDestroy()
{
	if(Root->path)
		XFREE(MTYPE_HTTP_CACHE_ROOT, Root->path);
}

/* 检查字符是否为文件系统安全字符. */
static int
fssafe(char c)
{
    if(c <= 31 || c >= 127)
        return 0;
    if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
       (c >= '0' && c <= '9') ||  c == '.' || c == '-' || c == '_')
        return 1;
    return 0;
}

/*检查缓存目录是否合法*/
static int CacheRootCheck(CacheRoot *CacheRoot)
{
    struct stat ss;
    int rc;

    if(!CacheRoot || CacheRoot->length == 0)
        return 0;

#ifdef WIN32  /* Require "x:/" or "x:\\" */
    rc = isalpha(CacheRoot->path[0]) && (CacheRoot->path[1] == ':') &&
         ((CacheRoot->path[2] == '/') || (CacheRoot->path[2] == '\\'));
    if(!rc) {
        return -2;
    }
#else
    if(CacheRoot->path[0] != '/') {
        return -2;
    }
#endif

    rc = stat(CacheRoot->path, &ss);
    if(rc < 0)
        return -1;
    else if(!S_ISDIR(ss.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    return 1;
}

static void
md5(unsigned char *restrict key, int len, unsigned char *restrict dst)
{
    static MD5_CTX ctx;
    MD5Init(&ctx);
    MD5Update(&ctx, key, len);
    MD5Final(&ctx);
    memcpy(dst, ctx.digest, 16);
}

/*
	@outbuf: 输出文件目录
*/
static int urlDirname(char *outbuf, int n, const char *url, int len)
{
    int i, j;
    if(len < 8)
        return -1;
	
    if(lwrcmp(url, "http://", 7) != 0)
        return -1;

	/*检查缓存目录是否合法*/
	if(CacheRootCheck(Root) <= 0)
		return -1;

	if(n <= Root->length)
		return -1;

    memcpy(outbuf, Root->path, Root->length);
    j = Root->length;

    if(outbuf[j - 1] != '/')
        outbuf[j++] = '/';

    for(i = 7; i < len; i++) {
        if(i >= len || url[i] == '/')
            break;
        if(url[i] == '.' && i != len - 1 && url[i + 1] == '.')
            return -1;
        if(url[i] == '%' || !fssafe(url[i])) {
            if(j + 3 >= n) return -1;
            outbuf[j++] = '%';
            outbuf[j++] = i2h((url[i] & 0xF0) >> 4);
            outbuf[j++] = i2h(url[i] & 0x0F);
        } else {
            outbuf[j++] = url[i]; if(j >= n) return -1;
        }
    }
    outbuf[j++] = '/'; 
	if(j >= n) 
		return -1;
    outbuf[j] = '\0';
    return j;
}


/* 
	根据URL，生成文件路径， https不生成文件名
	返回值: 
		文件路径的长度
*/
static int urlFilename(char *restrict buf, int n, const char *url, int len)
{
	int j;
	unsigned char md5buf[18];
	j = urlDirname(buf, n, url, len);
	if(j < 0 || j + 24 >= n)
		return -1;
	md5((unsigned char*)url, len, md5buf);
	b64cpy(buf + j, (char*)md5buf, 16, 1);
	buf[j + 24] = '\0';
	return j + 24;
}

/*创建文件，若目录不存在，先创建目录*/
static int
createFile(const char *name, int path_start)
{
    int fd;
    char buf[1024];
    int n;
    int rc;

    if(name[path_start] == '/')
        path_start++;

    if(path_start < 2 || name[path_start - 1] != '/' ) {
        fprintf(stderr, "Incorrect name %s (%d).\n", name, path_start);
        return -1;
    }

    fd = open(name, O_RDWR | O_CREAT | O_EXCL | O_BINARY, 0600);
    if(fd >= 0)
        return fd;
    if(errno != ENOENT) {
        fprintf(stderr, "Couldn't create disk file %s", name);
        return -1;
    }
    
    n = path_start;
    while(name[n] != '\0' && n < 1024) {
        while(name[n] != '/' && name[n] != '\0' && n < 512)
            n++;
        if(name[n] != '/' || n >= 1024)
            break;
        memcpy(buf, name, n + 1);
        buf[n + 1] = '\0';
        rc = mkdir(buf, 0700);
        if(rc < 0 && errno != EEXIST) {
            fprintf(stderr, "Couldn't create directory %s", buf);
            return -1;
        }
        n++;
    }
    fd = open(name, O_RDWR | O_CREAT | O_EXCL | O_BINARY,
	      0600);
    if(fd < 0) {
        fprintf(stderr, "Couldn't create file %s", name);
        return -1;
    }

    return fd;
}


/*
	保存CACHE到磁盘
*/
int CacheDiskStore(char *URI, int URI_len, unsigned char *data, unsigned int data_len)
{
	int name_len;	
	//FILE *fp = NULL;
	int fd = -1, rc;
	char buf[1024];

	if(data_len >= CACHE_DISK_STORE_SIZE_MAX)
	{
		fprintf(stdout, "Cache disk store failed:oversize cache(%u)\n", data_len);
		return -1;
	}
	
	/*生成缓存文件名*/
	name_len = urlFilename(buf, 1024, URI, URI_len);
	if(name_len <= 0)
	{
		fprintf(stderr, "urlFilename error.\n");
		return -1;
	}
	fprintf(stdout, "!!!!!!!!! %s\n", buf);
	

	fd = open(buf, O_RDWR | O_BINARY);
	if(fd >= 0)
	{
again1:
        rc = write(fd, data, data_len);

	    if(rc < 0 && errno == EINTR)
	        goto again1;

	    if(rc < data_len)
	        goto fail;		

		close(fd);
	}
	else
	{
		fd = createFile(buf, Root->length);
		if(fd < 0)
			goto fail;
again2:

		rc = write(fd, data, data_len);

	    if(rc < 0 && errno == EINTR)
	        goto again2;

	    if(rc < data_len)
	        goto fail;		

		close(fd);		
	}
	
	return 0;

fail:
	if(fd >= 0)
		close(fd);
	return -1;
}
/*检查cache是否存在*/
int CacheDiskExistCheckPath(char *CachePath)
{
	if (access(CachePath, F_OK) != 0)
		return 0;/*Not exist*/
	else
		return 1;/*exist in disk*/
		
}

/*检查cache是否存在*/
int CacheDiskExistCheckUri(char *URI, int URI_len)
{
	int name_len;	
	char buf[1024];
	/*生成缓存文件名*/
	name_len = urlFilename(buf, 1024, URI, URI_len);
	if(name_len < 0)
		return 0;/*Not exist*/
	
	return CacheDiskExistCheckPath(buf);
}


/*
	获取缓存
	返回值:
		< 0 -- 获取失败
		= 0 -- 缓存内容为空
		> 0 -- 缓存长度
*/
unsigned int CacheDiskGet(char *URI, int URI_len, unsigned char *out_buf, int buflen)
{
	int name_len;	
	FILE *fp = NULL;
	char buf[1024];

	memset(out_buf, 0, buflen);
	unsigned int cache_buffer_size;
	
	/*生成缓存文件名*/
	name_len = urlFilename(buf, 1024, URI, URI_len);
	if(name_len < 0)
		return -1;

	if(!CacheDiskExistCheckPath(buf))/*Cache文件不存在*/
		return -1;
	/*获取缓存*/
	fprintf(stdout, "Get cache from disk :%s\n", buf);
	
	fp = fopen(buf, "rb");
	if(NULL == fp)
	{
		fprintf(stdout, "Cache disk store failed, error to fopen file '%s': %s\n",
			buf, safe_strerror(errno));
		return -1;
	}

	fread(out_buf, buflen, 1, fp);
	fclose(fp);	

	cache_buffer_size = strlen((const char *)out_buf);
	
	return cache_buffer_size;
}

