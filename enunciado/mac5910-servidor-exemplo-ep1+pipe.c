/* Por Prof. Daniel Batista <batista@ime.usp.br>
 * Em 03/08/2025
 * 
 * Um código simples de um servidor de eco a ser usado como base para
 * o EP1. Ele difere do outro código de exemplo passado no enunciado
 * porque está usando um pipe para permitir que uma mesma mensagem
 * enviada pelo primeiro cliente conectado no servidor seja ecoada
 * para outros clientes conectados (pense bem nisso que esse código
 * está fazendo e você vai entender porque ele pode te ajudar na
 * implementação de um broker MQTT). Note que você não é obrigado a
 * usar esse código como base. O problema que esse código resolve pode
 * ser resolvido de diversas formas diferentes. Tudo que foi
 * adicionado nesse código em relação ao anterior está identificada
 * com comentários com dois asteriscos.
 * 
 * Ele recebe uma linha de um cliente e devolve a mesma linha.
 * Teste ele assim depois de compilar:
 * 
 * ./redes-servidor-exemplo-ep1+pipe 8000
 * 
 * Com este comando o servidor ficará escutando por conexões na porta
 * 8000 TCP (Se você quiser fazer o servidor escutar em uma porta
 * menor que 1024 você precisará ser root ou ter as permissões
 * necessárias para rodar o código com 'sudo').
 *
 * Depois conecte no servidor via telnet. Rode em outro terminal:
 * 
 * telnet 127.0.0.1 8000
 * 
 * Escreva sequências de caracteres seguidas de ENTER. Você verá que o
 * telnet exibe a mesma linha em seguida. Esta repetição da linha é
 * enviada pelo servidor. O servidor também exibe no terminal onde ele
 * estiver rodando as linhas enviadas pelos clientes.
 *
 * Mas o real poder desse código vai ser exibido quando você conectar
 * três clientes. Conecte os três, escreva mensagens apenas no
 * terminal do primeiro e você entenderá.
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

/** Para usar o mkfifo() **/
#include <sys/stat.h>
/** Para usar o open e conseguir abrir o pipe **/
#include <fcntl.h>

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

int main (int argc, char **argv) {
    int listenfd, connfd;
    struct sockaddr_in servaddr;
    pid_t childpid;
    char recvline[MAXLINE + 1];
    ssize_t n;

    /** Descritor de arquivo para o pipe **/
    int meu_pipe_fd[2];
    /** Nome do arquivo temporário que vai ser criado.
     ** TODO: isso é bem arriscado em termos de segurança. O ideal é
     ** que os nomes dos arquivos sejam criados com a função mkstemp e
     ** essas strings sejam templates para o mkstemp. **/
    char meu_pipe[2][27] = {"/tmp/temp.mac5910.1.XXXXXX", "/tmp/temp.mac5910.2.XXXXXX"};
    /** Para o loop de criação dos pipes **/
    int i;
    /** Variável que vai contar quantos clientes estão conectados.
     ** Necessário para saber se é o primeiro cliente ou não. **/
    int cliente;
    cliente = -2;
 
    if (argc != 2) {
        fprintf(stderr,"Uso: %s <Porta>\n",argv[0]);
        fprintf(stderr,"Vai rodar um servidor de echo na porta <Porta> TCP\n");
        exit(1);
    }

    /** Criando o pipe onde vou guardar as mensagens do primeiro
     ** cliente. Esse pipe vai ser lido pelos clientes seguintes. **/
    for (i=0;i<2;i++) {
        if (mkfifo((const char *) meu_pipe[i],0644) == -1) {
            perror("mkfifo :(\n");
        }
    }

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket :(\n");
        exit(2);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(atoi(argv[1]));
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind :(\n");
        exit(3);
    }

    if (listen(listenfd, LISTENQ) == -1) {
        perror("listen :(\n");
        exit(4);
    }

    printf("[Servidor no ar. Aguardando conexões na porta %s]\n",argv[1]);
    printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");
   
	for (;;) {
        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
            perror("accept :(\n");
            exit(5);
        }

        /** Para identificar cada cliente. Se for o primeiro, o
         ** funcionamento vai continuar como sendo de um cliente de echo. Se
         ** não for, ele vai receber as mensagens do primeiro cliente **/
        cliente++;
      
        if ( (childpid = fork()) == 0) {
            printf("[Uma conexão aberta]\n");
            close(listenfd);
         
            /** Se for o primeiro cliente, continua funcionando assim
             ** como estava antes, com a diferença de que agora, tudo que for
             ** recebido vai ser colocado no pipe também (o novo write
             ** no fim do while). Note que estou considerando que
             ** terão 2 clientes conectados. O primeiro, que vai ser o cliente de echo
             ** de fato, e o outro que só vai receber as mensagens do primeiro.
             ** Por isso que foi adicionado um open, um write e um close abaixo.
             ** Obs.: seria necessário um tratamento para o caso do
             ** primeiro cliente sair. Isso está faltando aqui mas não é necessário
             ** para o próposito desse exemplo. Além disso, precisa
             ** revisar se esse unlink está no lugar certo. A depender
             ** do SO, pode não ser ok fazer o unlink logo depois do open. **/
            if (cliente==-1) {
                meu_pipe_fd[0] = open(meu_pipe[0],O_WRONLY);
                meu_pipe_fd[1] = open(meu_pipe[1],O_WRONLY);
                unlink((const char *) meu_pipe[0]);
                unlink((const char *) meu_pipe[1]);
                while ((n=read(connfd, recvline, MAXLINE)) > 0) {
                    recvline[n]=0;
                    printf("[Cliente conectado no processo filho %d enviou:] ",getpid());
                    if ((fputs(recvline,stdout)) == EOF) {
                        perror("fputs :( \n");
                        exit(6);
                    }
                    write(connfd,         recvline, strlen(recvline));
                    write(meu_pipe_fd[0], recvline, strlen(recvline));
                    write(meu_pipe_fd[1], recvline, strlen(recvline));
                }
                close(meu_pipe_fd[0]);
                close(meu_pipe_fd[1]);
            }
            /** Se não for o primeiro cliente, passa a receber o pipe do primeiro cliente **/
            else if (cliente==0 || cliente==1) {
                meu_pipe_fd[cliente] = open(meu_pipe[cliente],O_RDONLY);
                while ((n=read(meu_pipe_fd[cliente], recvline, MAXLINE)) > 0) {
                    recvline[n]=0;
                    write(connfd,         recvline, strlen(recvline));
                }
                close(meu_pipe_fd[cliente]);
            }
            else
                close(connfd);

            printf("[Uma conexão fechada]\n");
            exit(0);
        }
        else
            close(connfd);
    }
    exit(0);
}
