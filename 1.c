nclude <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define send_type	1				// 2种 消息类型
#define recv_type	2

#define send_1_to_recv	1				// 4种 消息走向
#define send_2_to_recv	2
#define recv_to_send_1	3
#define recv_to_send_2	4

#define bool	int
#define false 	0
#define true	1


void *send_thread_1(void *arg);
void *send_thread_2(void *arg);
void *recv_thread(void *arg);
void P(sem_t *sem_ptr);
void V(sem_t *sem_ptr);

sem_t send_psx, recv_psx, final_recv_1, final_recv_2;	// 定义4个 信号量类型
pthread_t send_pid_1, send_pid_2, recv_pid;		// pthread_t 实际为 unsigned long int
int count = 1;						// 计数器，声明为全局	
bool send_1_over = false;				// sender1线程 是否已经结束
bool send_2_over = false;				// sender2线程 是否已经结束



struct msgbuf
{
    long mtype;
    char mtext[256];
    int mw2w;						// message where to where 表示消息的走向
};
int msgid;

int main(void)
{
    sem_init(&send_psx, 0, 1);				// pshared = 0，同一进程中多线程的同步
    sem_init(&recv_psx, 0, 0);
    sem_init(&final_recv_1, 0, 0);
    sem_init(&final_recv_2, 0, 0);

    msgid = msgget(IPC_PRIVATE, 0666|IPC_CREAT);	// 创建消息队列
    if (msgid < 0) {
        printf("[*] Error: msgget() return error\n");
        exit(1);
    }
    pthread_create(&send_pid_1, NULL, send_thread_1, NULL); // 创建 线程，并将线程加入当前 进程
    pthread_create(&send_pid_2, NULL, send_thread_2, NULL);
    pthread_create(&recv_pid, NULL, recv_thread, NULL);

    pthread_join(send_pid_1, NULL);			// 阻塞调用 send / receive 线程，功能相反于设置守护线程的 SetDaemon()
    pthread_join(send_pid_2, NULL);
    pthread_join(recv_pid, NULL);

    return 0;
}

void *send_thread_1(void *arg)
{
    char info[256];							
    struct msgbuf s_msg;				// 消息发送区
    s_msg.mtype = send_type;
    s_msg.mw2w = send_1_to_recv;
    while (1) {
        P(&send_psx);

	printf("[%d]\n", count);
        printf("[*] Send_thread_1 send: ");
        scanf("%s", info);
  
        if (strcmp(info, "exit") == 0) {
            strcpy(s_msg.mtext, "end1");
            msgsnd(msgid, &s_msg, sizeof(struct msgbuf), 0);
            V(&recv_psx);
            break;
        }
        strcpy(s_msg.mtext, info);
        count++;
        msgsnd(msgid, &s_msg, sizeof(struct msgbuf), 0);// 追加一条消息到消息队列中
        V(&recv_psx);
	
    }
    P(&final_recv_1);					// final_recv_1 处理 send_thread_1 最后一次接受消息的问题

    msgrcv(msgid, &s_msg, sizeof(struct msgbuf), recv_type, 0);	// 从消息队列中读一条消息
    printf("[*] Send_thread_1 receive: %s\n", s_msg.mtext);
    count++;

    V(&send_psx);					
  
    if (send_1_over && send_2_over){			//  2个 sender线程 都发送过 'end' 且收到过 'over' 后，将移除消息队列
	msgctl(msgid, IPC_RMID, 0);			// 移除消息队列
    }			
    pthread_exit(NULL);					// 类比进程的终止 exit()
}

void *send_thread_2(void *arg)
{
    char info[256];							
    struct msgbuf s_msg;				// 消息发送区
    s_msg.mtype = send_type;
    s_msg.mw2w = send_2_to_recv;
    while (1) {
        P(&send_psx);

	printf("[%d]\n", count);
        printf("[*] Send_thread_2 send: ");
        scanf("%s", info);
      
        if (strcmp(info, "exit") == 0) {
            strcpy(s_msg.mtext, "end2");
            msgsnd(msgid, &s_msg, sizeof(struct msgbuf), 0);
            V(&recv_psx);
            break;
        }
        strcpy(s_msg.mtext, info);
        count++;
        msgsnd(msgid, &s_msg, sizeof(struct msgbuf), 0);// 追加一条消息到消息队列中
        V(&recv_psx);

    }
    P(&final_recv_2);					// final_recv_2 处理 send_thread_2 最后一次接受消息的问题

    count++;
    msgrcv(msgid, &s_msg, sizeof(struct msgbuf), recv_type, 0);	// 从消息队列中读一条消息
    printf("[*] Send_thread_2 receive: %s\n", s_msg.mtext);	

    V(&send_psx);					

    if (send_1_over && send_2_over){			// 2个 sender 线程 都发送过 'end' 且收到过 'over' 后，将移除消息队列
	msgctl(msgid, IPC_RMID, 0);			// 移除消息队列
    }			
    pthread_exit(NULL);					// 类比进程的终止 exit()
}

void *recv_thread(void *arg)
{
    struct msgbuf r_msg;				// 消息接受区
    while (1) {
        P(&recv_psx);
        msgrcv(msgid, &r_msg, sizeof(struct msgbuf), send_type, 0);
	if (r_msg.mw2w == send_1_to_recv){		// 根据 消息走向 判断来源
            if (strcmp(r_msg.mtext, "end1") == 0) {
            	strcpy(r_msg.mtext, "over1");
            	r_msg.mtype = recv_type;
		r_msg.mw2w = recv_to_send_1;
            	msgsnd(msgid, &r_msg, sizeof(struct msgbuf), 0);
            	printf("[*] Recv_thread receive 'end1' from Send_thread_1, return 'over1'\n");

            	V(&final_recv_1);
                send_1_over = true;
            }
            else {
            	printf("[*] Recv_thread receive: %s from Send_thread_1\n", r_msg.mtext);
	    	V(&send_psx);
	    }
	}
	else if (r_msg.mw2w == send_2_to_recv) {	// 根据 消息走向 判断来源
            if (strcmp(r_msg.mtext, "end2") == 0) {
            	strcpy(r_msg.mtext, "over2");
            	r_msg.mtype = recv_type;
		r_msg.mw2w = recv_to_send_2;
            	msgsnd(msgid, &r_msg, sizeof(struct msgbuf), 0);
            	printf("[*] Recv_thread receive 'end2' from Send_thread_2, return 'over2'\n");

            	V(&final_recv_2);
		send_2_over = true;
            	
            }
            else {
                printf("[*] Recv_thread receive: %s from Send_thread_2\n", r_msg.mtext);
	        V(&send_psx);
	    }
	}
	

	if (send_1_over && send_2_over){		// 2个 sender线程 都发送过 'end' 且收到过 'over' 后，将跳出循环，结束当前线程
	    break;
	}
    }
    pthread_exit(NULL);
}

void P(sem_t *sem_ptr)
{
    sem_wait(sem_ptr);
}

void V(sem_t *sem_ptr)
{
    sem_post(sem_ptr);
}
