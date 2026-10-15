#include "mission_link.h"
#include <string.h>
#define MAGIC0 0x4Du
#define MAGIC1 0x4Cu
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static uint16_t get16(const uint8_t *p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static uint32_t get32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
uint16_t ml_crc16_ccitt(const uint8_t *data,size_t length){uint16_t crc=0xFFFFu;if(data==NULL&&length)return 0u;for(size_t i=0;i<length;i++){crc^=(uint16_t)data[i]<<8;for(unsigned b=0;b<8;b++)crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1);}return crc;}
size_t ml_packet_encode(const ml_packet_t *p,uint8_t *out,size_t cap){if(!p||!out||p->payload_length>ML_MAX_PAYLOAD)return 0u;size_t n=ML_HEADER_SIZE+(size_t)p->payload_length+ML_CRC_SIZE;if(cap<n)return 0u;out[0]=MAGIC0;out[1]=MAGIC1;out[2]=ML_PROTOCOL_VERSION;out[3]=p->type;out[4]=p->flags;out[5]=p->ttl;put32(out+6,p->source);put32(out+10,p->destination);put32(out+14,p->message_id);put16(out+18,p->payload_length);if(p->payload_length)memcpy(out+20,p->payload,p->payload_length);put16(out+n-2,ml_crc16_ccitt(out,n-2));return n;}
bool ml_packet_decode(const uint8_t *f,size_t n,ml_packet_t *out){if(!f||!out||n<ML_HEADER_SIZE+ML_CRC_SIZE||n>ML_MAX_FRAME)return false;if(f[0]!=MAGIC0||f[1]!=MAGIC1||f[2]!=ML_PROTOCOL_VERSION)return false;uint16_t len=get16(f+18);if(len>ML_MAX_PAYLOAD||n!=ML_HEADER_SIZE+(size_t)len+ML_CRC_SIZE)return false;if(get16(f+n-2)!=ml_crc16_ccitt(f,n-2))return false;ml_packet_t p={0};p.type=f[3];p.flags=f[4];p.ttl=f[5];p.source=get32(f+6);p.destination=get32(f+10);p.message_id=get32(f+14);p.payload_length=len;if(len)memcpy(p.payload,f+20,len);*out=p;return true;}
