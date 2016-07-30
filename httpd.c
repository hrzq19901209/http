#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

#define ISSPace(x) isspace((int)(x))
#define SERVER_STRING "Server: httpd/0.1.0\r\n"

void error_die(const char *sc);
void not_found(int client);
void unimplemented(int client);
int get_line(int sock, char *buf, int size);
void headers(int client, const char *filename);
void cat(int client, FILE *resource);
void serve_file(int client, const char* filename);
void accept_request(void *arg);
int startup(u_short *port);

void error_die(const char *sc){
    perror(sc);
    exit(1);
}

void not_found(int client){
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void unimplemented(int client){
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

int get_line(int sock, char *buf, int size){
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n')){
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0){
            if (c == '\r'){//读到\r,\r\m就结束，并且每行以\n结束
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return i;
}

void cat(int client, FILE *resource){
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

void headers(int client, const char *filename){
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

void serve_file(int client, const char* filename){
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];
    
    buf[0] = 'A';
    buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf)){
        numchars = get_line(client, buf, sizeof(buf));
    }
    
    resource = fopen(filename, "r");
    if(resource == NULL){
        printf("not found ser\n");
        not_found(client);
    }else{
        headers(client, filename);
        cat(client, resource);
    }   
    fclose(resource);
}

void accept_request(void *arg){
    int client = *(int*)arg;
    char buf[1024];

    size_t numchars;
    char method[255];
    char url[255];
    char path[512];
    char *query_string = NULL;
    struct stat st;

    size_t i, j;
    numchars = get_line(client, buf, sizeof(buf));
    i = 0;
    while(!ISSPace(buf[i]) && i < numchars){
        method[i] = buf[i];
        i++;    
    }
    method[i] = '\0';

    if(strcasecmp(method, "GET") && strcasecmp(method, "POST")){//只处理GET和POST请求
        unimplemented(client);    
        return;    
    }

    j = 0;
    while(ISSPace(buf[i]) && i < numchars){
        i++;
    }
    while(!ISSPace(buf[i]) && i < numchars){
        url[j] = buf[i];
        i++;
        j++;
    }
    url[j] = '\0';

    if(strcasecmp(method, "GET") == 0){
        query_string = url;
        while((*query_string != '?') && (*query_string != '\0')){
            query_string++;    
        }
        if(*query_string == '?'){
            *query_string = '\0';
            query_string++;    
        }
    }

    sprintf(path, "htdcos%s", url);
    if(path[strlen(path)-1] == '/'){
        strcat(path, "index.html");
    }

    if(stat(path, &st) == -1){
        while(numchars > 0 && strcmp("\n", buf)){
            numchars = get_line(client, buf, sizeof(buf));    
        }
        not_found(client);
        printf("not found acc\n");
    }else{
        if((st.st_mode & S_IFMT) == S_IFDIR){
            strcat(path, "/index.html");
        }
        serve_file(client, path);
    }
    printf("method: %s\n", method);
    printf("url: %s\n", url);
    printf("path: %s\n", path);
    close(client);
}

int startup(u_short *port){
    int httpd = 0;
    struct sockaddr_in name;

    httpd = socket(AF_INET, SOCK_STREAM, 0);        
    if(httpd == -1){
        error_die("create httpd socket failed!");    
    }

    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);//<netinet/in.h>

    if(bind(httpd, (struct sockaddr*)&name, sizeof(name)) == -1){
        error_die("bind httpd failed");
    }

    if(*port == 0){//系统自动产生port
        socklen_t namelen = sizeof(name);
        if(getsockname(httpd, (struct sockaddr*)&name, &namelen) == -1){
            error_die("getsockname failed");    
        }
        *port = ntohs(name.sin_port);
    }

    if(listen(httpd, 5) == -1){
        error_die("listen failed");    
    }
    return httpd;
}

int main(int argc, char* argv[]){
    int server_sock = -1;
    u_short port = 4000;
    int client = -1;

    struct sockaddr_in client_name;
    socklen_t client_name_len = sizeof(client_name);
    pthread_t newthread;

    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);
    
    while(1){
        client = accept(server_sock, (struct sockaddr *)&client_name, &client_name_len);
        if(client == -1){
            error_die("accpet");    
        }
        if(pthread_create(&newthread, NULL, (void *)accept_request, (void *)&client) != 0){
            perror("pthread_create");    
        }
    }
    close(server_sock);
    return 0;
}
