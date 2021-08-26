#include "sys.h"
#include "delay.h"
#include "usart.h" 
#include "led.h" 		 	 

#include "uc8088_spi.h"
#include "wdg.h"

#include <stdbool.h>

volatile bool which_buf = 0;
volatile u16 str_len[2] = {0};
u8 Buffer[2][SPI_BUF_LEN] = {0};

volatile u8  rp_OK, wp_OK, cnt, send_flag=1;
volatile u16 tmp, tmp1, number, ret;
volatile unsigned int rrp, wp, rp;

void ByteChange(register u8 *pBuf, s16 len)
{
	register s16 i;
	register u8 ucTmp;
	
	for(i = 0; i < len; i += 4)
	{
			ucTmp = pBuf[i+3];
			pBuf[i+3] = pBuf[i];
			pBuf[i] = ucTmp;
			ucTmp = pBuf[i+2];
			pBuf[i+2] = pBuf[i+1];
			pBuf[i+1] = ucTmp;
	}
}

void ML302_init()
{
	printf("AT\r\n");			
	delay_ms(10);
	printf("AT+CPIN?\r\n");		//查询SIM卡状态
	delay_ms(10);
	printf("AT+CSQ\r\n");			//查询信号质量， 小于10说明信号差
	delay_ms(10);
	printf("AT+CGACT=1,1\r\n");		//激活PDP
	delay_ms(20);
	printf("AT+MIPOPEN=1,\"TCP\",\"server.natappfree.cc\",43328\r\n");	//连接服务器
	delay_ms(20);
	
	delay_ms(10);
	printf("ATE0\r\n");				//关闭回显
	delay_ms(10);
	memset(USART_RX_BUF, 0, USART_REC_LEN);
	USART_RX_STA = 0;
}

void uart_send_data_2_ML302(register u8 *TX_BUF, u16 len)
{
	char num_char[5];
	register u16 i;
	sprintf(num_char, "%d", len);
	printf("AT+MIPSEND=1,%s\r\n", num_char);
	//TX_BUF[len] = '\0';
	//printf("%s", TX_BUF);
	for(i=0; i<len; i++){
		USART1->DR = TX_BUF[i];
		while((USART1->SR&0X40)==0);//等待该字符发送完毕   
	}
}

void uart_send_data(register u8 *TX_BUF, u16 len)
{
	register int i;
	
	for(i=0; i<len; i++){
		USART1->DR = TX_BUF[i];
		while((USART1->SR&0X40)==0);//等待该字符发送完毕   
	}
}

u32 read_test()
{
	return uc8088_read_u32(0x1a107018);
}

u16 get_rp_wp_value()
{
		cnt = 0;
		do
		{
			uc8088_read_2_u32(Buf_addr, &wp, &rp);
			if (rp == rrp && wp < 1024)
			{
				rp_OK = 1;
				number = (rp <= wp) ? (wp - rp) : (1024 - rp + wp);
				if (number < 32){
					wp_OK = 0;
					rp_OK = 0;
				}
				else if((number*4 + str_len[which_buf]) > SPI_BUF_LEN){
					wp_OK = 0;
					rp_OK = 0;
				}
				else 
					wp_OK = 1;
				break;
			}
			
			if(cnt++ > 3)
			{
				printf("read rp and wp error! rrp = %d,  rp = %d,  wp = %d!\r\n", rrp, rp, wp);
				if (((rp == 0xffffffff) && (wp == 0xffffffff)) || ((rp == 0) && (wp == 0)))
						return 1;
				else
						return 2;
			}
		}while(1);		// 读取 读写指针位置
		return 0;
}

void read_memory()
{
			//printf("wp = %d,   rrp = %d,  rp = %d\r\n", wp, rrp, rp);
			if (rp < wp){
				tmp = uc8088_read_memory(Buf_addr + 8 + rp*4, Buffer[which_buf] + str_len[which_buf], number*4);
				tmp1 = rp*4+tmp;
				str_len[which_buf] += tmp;
			}
			else{
				tmp = uc8088_read_memory(Buf_addr + 8 + rp*4, Buffer[which_buf]+ str_len[which_buf], SPI_BUF_LEN - rp*4);
				str_len[which_buf] += tmp;
				tmp1 = uc8088_read_memory(Buf_addr + 8, Buffer[which_buf]+ str_len[which_buf], wp*4);
				str_len[which_buf] += tmp1;
			}
			cnt = 0;  	rp_OK=0;		wp_OK=0;  rrp = 65536;
}

u16 change_rp()
{
	cnt = 0;
	do{
			uc8088_write_u32(Buf_addr + 4, tmp1>>2);
			rrp = uc8088_read_u32(Buf_addr + 4);
			
			if(cnt++ > 3)		//修改读指针失败
			{
				printf("write rp = %d, not eq %d, so is error!\r\n", rrp, tmp1>>2);
				return 1;
			}
	}while(rrp != tmp1>>2);		// 修改读指针
	return 0;
}


int main(void)
{
	delay_init();	    	 //延时函数初始化	  
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
	uart_init(230400);	 	//串口初始化为230400
 	LED_Init();		  			//初始化与LED连接的硬件接口
	//KEY_Init();					//初始化按键
	uc8088_init();		//8088初始化
	
	printf("halt cpu\t");
	rp = uc8088_read_u32(0x1a107018);
	printf("read test = %x\r\n", rp);

	wp = uc8088_read_u32(Buf_addr);
	rp = uc8088_read_u32(Buf_addr + 4);
	printf("wp = %d,   rp = %d\r\n", wp, rp);
	LED0 = 1;
	ML302_init();
	wp = 65536;
	rrp = rp;
	
	begin:
	wp_OK = rp_OK = 0;
	if(read_test() != 0xabcd0001)
		goto loop;
	rrp = uc8088_read_u32(Buf_addr + 4);

	while(1){	
		
		if(rp_OK == 1 && wp_OK == 1){			// 读取内存
				LED0 = 0;
				read_memory();
				if(change_rp())
					goto loop;
				LED0 = 1;
		}
		else{
			ret = get_rp_wp_value();	// 获取读写指针位置
			if (ret == 1)
				goto end;
			else if (ret == 2)
				goto loop;
		}
		
		if(str_len[which_buf] > 128)	// uart 打印内容
		{
			LED1 = 0;
			uart_send_data((unsigned char *) Buffer[which_buf], str_len[which_buf]);
			which_buf = !which_buf;
			str_len[which_buf] = 0;
			LED1 = 1;
		}
		
	}
	loop:
	rrp = 5;
	LED0 = 0;
	while(rrp--)		// 暂时SPI读取异常
	{
		LED1 = !LED1;
		
		delay_ms(100);
		uc8088_init();		//8088初始化
		if(read_test() == 0xabcd0001){
			goto begin;
		}
	}
	goto begin;
	end:
	while(1)		// 线被拔掉了
	{
		LED1 = !LED1;
		delay_ms(500);
		uc8088_init();		//8088初始化
		if(read_test() == 0xabcd0001){
			goto begin;	
		}
	}
}


//int main(void)
//{		
//	volatile u8  rp_OK, wp_OK, cnt, send_flag=1;
//	volatile u16 tmp, tmp1, len, i;
//	volatile u32 rrp;
//	u32 wp, rp;

//	delay_init();	    	 //延时函数初始化	  
//  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
//	uart_init(230400);	 	//串口初始化为230400
// 	LED_Init();		  			//初始化与LED连接的硬件接口
//	//KEY_Init();					//初始化按键
//	uc8088_init();		//8088初始化
//	
//	printf("halt cpu\t");
//	rp = uc8088_read_u32(0x1a107018);
//	printf("read test = %x\r\n", rp);

//	wp = uc8088_read_u32(Buf_addr);
//	rp = uc8088_read_u32(Buf_addr + 4);
//	printf("wp = %d,   rp = %d\r\n", wp, rp);
//	LED0 = 1;
//	ML302_init();
//	wp = 65536;
//	rrp = rp;

//	//IWDG_Init(5,625);    //与分频数为128,重载值为625,溢出时间为2s
//	//send_flag = 1;
//	printf("begin while 1\r\n");
//	while(1){
//		cnt = 0;
//		do
//		{
//			uc8088_read_2_u32(Buf_addr, &wp, &rp);
//			if (rp == rrp && wp < 1024)
//			{
//				rp_OK = 1;
//				len = (rp <= wp) ? (wp - rp) : (1024 - rp + wp);
//				if (len < 64){
//					wp_OK = 0;
//				}
//				else if((len*4 + str_len[which_buf]) > SPI_BUF_LEN)
//					wp_OK = 0;
//				else 
//					wp_OK = 1;
//				break;
//			}

//			if(cnt++ > 5)
//			{
//				wp_OK = 0;
//				printf("rrp = %d,  rp = %d,  wp = %d!\r\n", rrp, rp, wp);
//				break;
//			}
//		}while(1);
//		
//		if(rp_OK == 1 && wp_OK == 1){
//			LED1 = 0;
//			
//			if (rp < wp){
//				tmp = uc8088_read_memory(Buf_addr + 8 + rp*4, Buffer[which_buf] + str_len[which_buf], len*4);
//				tmp1 = rp*4+tmp;
//				str_len[which_buf] += tmp;
//			}
//			else{
//				tmp = uc8088_read_memory(Buf_addr + 8 + rp*4, Buffer[which_buf]+ str_len[which_buf], SPI_BUF_LEN - rp*4);
//				tmp1 = uc8088_read_memory(Buf_addr + 8, Buffer[which_buf]+ str_len[which_buf] + tmp, wp*4);
//				tmp = tmp + tmp1;
//				str_len[which_buf] += tmp;
//			}
//			//printf("str_len[%d] = %d,  tmp = %d\r\n", which_buf, str_len[which_buf], tmp);
//			cnt = 0;  	rp_OK=0;		wp_OK=0;  rrp = 65536;
//			
//			do{
//					uc8088_write_u32(Buf_addr + 4, tmp1>>2);
//					rrp = uc8088_read_u32(Buf_addr + 4);
//					
//					if(cnt++ > 5)		//修改读指针失败
//					{
//						printf("write rp = %d, not eq %d, so is error!\r\n", rrp, tmp1>>2);
//						break;
//					}
//			}while(rrp != tmp1>>2);
//		}
//		
//		//收到1KB以上内容就发给ML302
//		if(str_len[which_buf] > 1024)
//		{
////			for (i=0; i < str_len[which_buf]; i++)
////					printf("%u,", *(Buffer[which_buf] + i));
////			printf("%s", (char *)*(Buffer[which_buf]));
//			uart_send_data((unsigned char *) Buffer[which_buf], str_len[which_buf]);
//			which_buf = !which_buf;
//			str_len[which_buf] = 0;
//			
//			//IWDG_Feed();		//喂狗
//		}

//	}
//}
