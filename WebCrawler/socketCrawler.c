#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr

#include <unistd.h>
#include <pthread.h>

int main(int argc, char *argv[]){

    int sock_desc;
    struct sockaddr_in servidor;
    char *msg, resp_servidor[10000];

    //Criar o socket
    //socket(dominio, tipo de socket, protocolo)
    //AF_INET = ipv4, SOCK_STREAM = TCP
    sock_desc = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_desc == -1){
        printf("Nao foi possivel criar o socket!");
    }

    //definir a estrutura do socket servidor
    //servidor.sin_family = AF_INET;
    servidor.sin_family = inet_addr("172.217.28.4");
    servidor.sin_addr.s_addr = INADDR_ANY;
    servidor.sin_port = htons(80); //port 80 é onde a maioria dos webservers rodam

    //Conectar a um servidor remoto
    if(connect(sock_desc , (struct sockaddr *)&servidor , sizeof(servidor)) < 0){
        printf("Nao foi possivel estabelecer a conexao!");
        return 0;
    }
    else{
        printf("Conexao estabelecida com sucesso!\n");
    }

    //Enviar dados ao servidor
    msg = "GET / HTTP/1.1\r\n\r\n"; //comando HTTP para pegar a pagina principal de um website
    if(send(sock_desc, msg, strlen(msg), 0) < 0){
        printf("Nao foi possivel enviar os dados!");
        return 0;
    }
    else{
        printf("Os dados foram enviados com sucesso!\n");
    }

    //Receber resposta do servidor
    if(recv(sock_desc, resp_servidor, 10000, 0) < 0){
        printf("Nao foi possivel recuperar a resposta do servidor!");
        return 0;
    }
    else{
        printf("Resposta recuperada com sucesso!\n");
    }
    puts(resp_servidor);

    return 0;
}
