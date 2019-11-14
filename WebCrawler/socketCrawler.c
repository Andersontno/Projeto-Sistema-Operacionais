#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strlen
#include <unistd.h> //close
#include <sys/socket.h>
#include <netdb.h> //struct addrinfo e função getaddrinfo
#include <pthread.h>
#include <sys/stat.h>
#include "socketCrawler.h"

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#define TRUE 1
#define FALSE 0
#define LENBUFFER 312

//definir a estrutura do socket servidor
struct addrinfo criarServidor(struct addrinfo hints, struct addrinfo **res, char *endereco){
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(endereco, "80", &hints, res)!=0){
        perror("getaddrinfo");
    }


    return **res;
}

//Criar o socket
//socket(dominio, tipo de socket, protocolo)
//AF_INET = ipv4, SOCK_STREAM = TCP, protocolo 0 é o padrão
int criarSocket(int *sock_desc, struct addrinfo *res){

    *sock_desc = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(*sock_desc == -1){
        printf("Nao foi possivel criar o socket!");
        return *sock_desc;
    }
    return *sock_desc;
}

void conversarServidor(int sock_desc, struct addrinfo *res, char *endereco, char *subEndereco, FILE *fp){
    char msg[512]; //msg = mensagem que será enviada ao servidor
    char resp_servidor[1024]; //variável que irá receber a resposta do servidor
    int bytes_read; //variável de suporte para capturar a resposta do servidor

    //Enviar dados ao servidor
    //msg = "GET /index.html HTTP/1.1\r\nHost: www.site.com\r\n\r\n"; comando HTTP para pegar a pagina principal de um website
    //index.html fica subentendido quando não se coloca nada após o primeiro /
    //Host: precisa ser especificado pois vários endereços podem utilizar o mesmo servidor ip
    //Connection: close simplesmente fecha a conexão após a resposta do servidor ser enviada
    if(subEndereco == NULL){
        strcpy(msg, "GET / HTTP/1.1\nHost: ");
    }else{
        strcpy(msg, "GET ");
        strcat(msg, subEndereco);
        strcat(msg, " HTTP/1.1\nHost: ");
    }
    
    strcat(msg, endereco);
    strcat(msg, "\r\nConnection: close\n\n");
    printf("%s\n",msg);
    if(send(sock_desc, msg, strlen(msg), 0) < 0){
        printf("Nao foi possivel enviar os dados!");
    }
    else{
        printf("Os dados foram enviados com sucesso!\n");
    }

    //Receber resposta do servidor
    //recv(descritor do socket, variável onde será armazenada a resposta do servidor, tamanho da variável, flags, padrão 0)
    //do while usado para que enquanto a resposta do servidor não for completamente capturada, continuar pegando o que falta
    do{
        bytes_read = recv(sock_desc, resp_servidor, 1024, 0);

        if(bytes_read == -1){
            perror("recv");
        }
        else{
            fprintf(fp, "%.*s", bytes_read, resp_servidor);
        }
    } while(bytes_read > 0);
    printf("%s\n",resp_servidor);
}

//Conectar a um servidor remoto e conversar com ele
void conectarServidor(int sock_desc, struct addrinfo *res, char *endereco, char *subEndereco, FILE *fp){
    //connect(descritor do socket, struct do servidor, tamanho do servidor
    if(connect(sock_desc , res->ai_addr , res->ai_addrlen) < 0){
        printf("Nao foi possivel estabelecer a conexao!");
        perror("connect");
    }
    else{
        printf("Conexao estabelecida com sucesso!\n");
    }

    freeaddrinfo(res);
    conversarServidor(sock_desc, res, endereco, subEndereco, fp);
}

///////////////////////////////////////////////////////////////////////////////////////////
//definir a estrutura do socket servidor SSL
struct addrinfo criarServidorSSL(struct addrinfo hints, struct addrinfo **res, char *endereco){
    char end[500] = "https://";
    strcat(end, endereco);

    /*
    ###################################################################################
    ###################################################################################
    char protocolo[10] = "";
    char host[500] = "";
    int port;
    char *pointer = NULL;

    //remove o / no final do link caso exista
    if(end[strle(end)] == '/'){
        end[strlen(end)] = '\0';
    }

    //O primeiro : termina a string do protocolo
    strncpy(protocolo, end, (strchr(end, ':')-end));

    //O hostname começa após o ://
    strncpy(host, strstr(url_str, "://")+3, sizeof(host));

    //Se o host possui um :, capturar o port
    if(strchr(host, ':')){
        pointer = strchr(host, ':');
        //O ultimo : começa o port
        strcnpy(port, pointer+1, sizeof(port));
        *pointer = '\0';
    }

    port = atoi(port);

    if((host = gethostbyname(host)) == NULL){
        BIO_printf(outbio, "Não foi possivel capturar o host %s.\n", host);
        abort();
    }
    ###################################################################################
    ###################################################################################
    */

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(end, "443", &hints, res)!=0){
        perror("getaddrinfo");
    }


    return **res;
}

//Criar o socket SSL
//socket(dominio, tipo de socket, protocolo)
//AF_INET = ipv4, SOCK_STREAM = TCP, protocolo 0 é o padrão
int criarSocketSSL(int *sock_desc, struct addrinfo *res, char *endereco, BIO *bioSaida){
    *sock_desc = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(*sock_desc == -1){
        printf("Nao foi possivel criar o socket!");
        return *sock_desc;
    }  
    return *sock_desc;
}

int conectarServidorSSL(int *sock_desc,struct addrinfo *res,char *endereco,char *subEndereco,FILE *fp){
    BIO *bioSaida = NULL;
    BIO *bioCertificado = NULL;
    BIO *web;
    X509 *certificado = NULL;
    X509_NAME *nomeCerificado = NULL;
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    SSL *ssl;
    int servidor = 0;
    
    //As funções abaixo inicializam as funcionalidades necessárias da biblioteca openssl
    OpenSSL_add_all_algorithms();
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();
    SSL_load_error_strings();

    //Cria as BIOs de input e output
    bioSaida = BIO_new_fp(stdout, BIO_NOCLOSE);

    //Inicializa a biblioteca SSL
    if(SSL_library_init() < 0){
        BIO_printf(bioSaida, "Não foi possível iniciar a biblioteca OpenSSL!\n");
    }

    //Abaixo estão sendo implementados os passos para a criação objeto context, o SSL_CTX
    //Este objeto é usado para criar um novo objeto de conexão para cada conexão SSL, objetos
    //de conexão são usados para realizar handshakes, escritas e leituras

    //Faz com que o handshake tente utilizar o protocolo SSLv2, mas também coloca como opções 
    //o SSLv3 e TLSv1
    method = SSLv23_client_method();

    //Cria o objeto context
    if((ctx = SSL_CTX_new(method)) == NULL){
        BIO_printf(bioSaida, "Não foi possivel criar um objeto SSL!\n");
    }

    //Caso o protocolo SSLv2 não funcione, tenta negociar com SSLv3 e o TSLv1
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);

    //Cria o objeto de estado SSL
    ssl = SSL_new(ctx);

    web = BIO_new_ssl_connect(ctx);
    BIO_get_ssl(web, &ssl);

	
    /*
    ###################################################################################
    ###################################################################################
    //Cria a conexão TCP
    servidor = criarSocketSSL(hints, pres, res->ai_ddr);
    if(servidor == 0){
        BIO_printf(outbio, "Não foi possível criar a conexão TCP");
    }
    ###################################################################################
    ###################################################################################
    */

    //Conecta a sessão SSL com o descritor do socket
    SSL_set_fd(ssl, servidor);

    //Realiza a conexão SSL
    if(SSL_connect(ssl)!=1){
        BIO_printf(outbio, "Não foi possivel reazilar a conexão SSL");
    }

    //Pegar o certificado remoto e o inserir na estrutura x509
    certificado = SSL_get_peer_certificate(ssl);
    if(cert == NULL){
        BIO_printf(outbio, "Não foi possivel recuperar o certificado do site");
    }

    //Extrair informações do certificado
    nomeCertificado = X509_NAME_new();
    nomeCertificado = X509_get_subject_name(certificado);

    //Realizar a requisição HTTP para baixar as informações do site
    char msg[512]; //msg = mensagem que será enviada ao servidor
    char resp_servidor[1024]; //variável que irá receber a resposta do servidor
    int bytes_read; //variável de suporte para capturar a resposta do servidor

    //Enviar dados ao servidor
    //msg = "GET /index.html HTTP/1.1\r\nHost: www.site.com\r\n\r\n"; comando HTTP para pegar a pagina 	    principal de um website
    //index.html fica subentendido quando não se coloca nada após o primeiro /
    //Host: precisa ser especificado pois vários endereços podem utilizar o mesmo servidor ip
    //Connection: close simplesmente fecha a conexão após a resposta do servidor ser enviada
    if(subEndereco == NULL){
        strcpy(msg, "GET / HTTP/1.1\nHost: ");
    }else{
        strcpy(msg, "GET ");
        strcat(msg, subEndereco);
        strcat(msg, " HTTP/1.1\nHost: ");
    }
    
    strcat(msg, endereco);
    strcat(msg, "\r\nConnection: close\n\n");
    printf("%s\n",msg);
    BIO_puts(web, msg);
    BIO_puts(outbio, "\n");

    //Receber resposta do servidor
    //recv(descritor do socket, variável onde será armazenada a resposta do servidor, tamanho da variável, flags, padrão 0)
    //do while usado para que enquanto a resposta do servidor não for completamente capturada, continuar pegando o que falta
    int tam = 0;
    do{
        tam = BIO_read(web, resp_servidor, sizeof(resp_servidor));

        if(tam>0){
            BIO_write(outbio, resp_servidor, tam);

    }while(tam>0 || BIO_should_retry(web));

    if(outbio){
        BIO_free(outbio);
    }

    if(web != NULL){
        BIO_free_all(web);
    }

    //Liberar as estruturas que não serão mais usadas
    SSL_free(ssl);
    close(servidor);
    X509_free(certificado);
    SSL_CTX_free(ctx);
     
}

int salvar_link_visitado(char *link, char *dominio){
    char *file_name = "linksVisitados.txt", *path = get_path(dominio, file_name);

    FILE *arquivo = fopen(path,"a");
    int status = TRUE;

    printf("============================================\n");  
    if(arquivo == NULL){
        printf("ERRO AO ADICIONAR LINK NO ARQUIVO!!!\n");
        status = FALSE;
    }else{
        fprintf(arquivo,"%s\n", link);  
        fclose(arquivo);      
        printf("LINK: %s VISITADO!!!\n", link);
    }
    printf("============================================\n");    
    return status;
}

ListaLinks* listar_links_visitados(char *dominio){
    ListaLinks *lista = startLista();
    char *buffer_link = malloc(LENBUFFER*sizeof(char));
    char *file_name = "linksVisitados.txt", *path = get_path(dominio, file_name);

    FILE *arquivo = fopen(path,"a");
    
    if(arquivo != NULL){
        while(fgets(buffer_link, LENBUFFER, arquivo) != NULL){
            addLista(lista, buffer_link);
            buffer_link = malloc(LENBUFFER*sizeof(char));
        }
        fclose(arquivo);
    }

    return lista;
}

int link_visitado(char *link, char *dominio){
    int boolean = FALSE;
    ListaLinks *lista = listar_links_visitados(dominio);
    char *link_lista = pop(lista);
    
    while (link_lista != NULL & boolean != TRUE){
        if(strcmp(link, link_lista) == 0){//funcao strcmp retorna 0(zero) quando as strings iguais
            boolean = TRUE;
        }
        else{
            link_lista = pop(lista);
        }
    }
    
    free_lista(lista);
    
    return boolean;
}

void *baixar_pagina(void *args){

    Arg_download *arg = (Arg_download*)args;

    int sock_desc; //descritor do socket
    int *psock = &sock_desc;
    struct addrinfo hints, *res;
    struct addrinfo **pres = &res;

    char *path = get_path(arg->endereco, arg->nome_arquivo_saida);
    
    FILE *fp; //arquivo onde será armazenado a resposta do servidor
    fp = fopen(path, "w");

    criarServidor(hints, pres, arg->endereco);
    criarSocket(psock, res);    
    conectarServidor(sock_desc, res, arg->endereco, arg->subEndereco, fp);

    fclose(fp);
    close(sock_desc);
}

void percorrer_links(char* dominio){
    char *buffer_link = malloc(LENBUFFER*sizeof(char));
    char *nome_arquivo_saida = malloc(LENBUFFER*sizeof(char)), *temp = malloc(LENBUFFER*sizeof(char));
    char *arq_links = "linksEncontrados.txt", *path = get_path(dominio, arq_links);
    int contador = 1;

    FILE *arquivoLinks = fopen(path,"r");

    if(arquivoLinks != NULL){
        while(fgets(buffer_link, LENBUFFER, arquivoLinks) != NULL){
            if(!link_visitado(buffer_link, dominio)){
                buffer_link[strlen(buffer_link)-1] = '\0';
                snprintf(temp, 10, "%d", contador);//converte int em string

                strcpy(nome_arquivo_saida,"link ");
                strcat(nome_arquivo_saida,temp);
                strcat(nome_arquivo_saida,".html");

                Arg_download *args = start_arg(dominio, buffer_link, nome_arquivo_saida);
                pthread_t thread;
                pthread_create(&thread,NULL,baixar_pagina,(void*)args);
                pthread_join(thread,NULL);
                contador++;
                
                salvar_link_visitado(buffer_link, dominio);
            }
        }
        fclose(arquivoLinks);
    }
}

Arg_download* start_arg(char *endereco, char *subEndereco, char* nome_arquivo_saida){
    Arg_download *arg = malloc(sizeof(Arg_download));
    
    arg->endereco = endereco;
    arg->subEndereco = subEndereco;
    arg->nome_arquivo_saida = nome_arquivo_saida;

    return arg;
}

void criar_pasta_dominio(char *dominio){
    int result = mkdir(dominio, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    // Caso a pasta seja criada o retorno será 0(zero).
    printf("============================================\n");  
    if(!result){
        printf("DIRETORIO %s CRIADO COM SUCESSO!!!\n",dominio);
    }else{
        printf("FALHA AO CRIAR DIRETORIO!!!\n");
    }
    printf("============================================\n");
}

void *percorrer_dominio(void *dominio){
    char *end = (char*)dominio; //endereço do site a ser visitado
    char* nome_arquivo_saida = "site.html";

    char *path = get_path(end, nome_arquivo_saida);

    criar_pasta_dominio(end);
        
    Arg_download *arg_site = start_arg(end, NULL,nome_arquivo_saida);

    pthread_t thread;
    pthread_create(&thread,NULL,baixar_pagina,(void*)arg_site);
    pthread_join(thread,NULL);
        
    ListaLinks *lista = filtrar_lista(buscarLinks(path),end);

    print_lista(lista);
    salvar_links_econtrados(lista,end);
    percorrer_links(end);
}
