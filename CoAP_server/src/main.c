#include <zephyr/zephyr.h>
#include <zephyr/drivers/gpio.h>
#include <device.h>
#include <devicetree.h>
#include <net/openthread.h>
#include <openthread/thread.h>
#include <openthread/coap.h>
 
/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/*
Configuring button specifications
*/
#define BUTTON0_NODE	DT_NODELABEL(button0)

static const struct gpio_dt_spec button0_spec = GPIO_DT_SPEC_GET(BUTTON0_NODE, gpios);
static struct gpio_callback button0_cb;

static void store_data_request_cb(void *p_context, otMessage *p_message, const otMessageInfo *p_message_info);

static void store_data_response_send( otMessage *p_message, const otMessageInfo *p_message_info);

static void get_data_request(void);

#define TEXT_BUFFER_SIZE 30
char myText[TEXT_BUFFER_SIZE];
uint16_t my_text_length = 0;

static otCoapResource m_storedata_resource = {
	.mUriPath = "storedata",
	.mHandler = store_data_request_cb,
	.mContext = NULL,
	.mNext = NULL
};

static void store_data_request_cb(void *p_context, otMessage *p_message, const otMessageInfo *p_message_info)
{
	otCoapCode messageCode = otCoapMessageGetCode(p_message);
	otCoapType messageType = otCoapMessageGetType(p_message);
	uint16_t messageId = otCoapMessageGetMessageId(p_message);

	do  {
		
		// if the message is of acknowledgment or delete type program exits
		if (messageType != OT_COAP_TYPE_NON_CONFIRMABLE && messageType != OT_COAP_TYPE_CONFIRMABLE){
			break;
		}

		// if message code is not of put type program exits
		if(messageCode != OT_COAP_CODE_PUT){
			break;
		}

		my_text_length = otMessageRead(p_message, otMessageGetOffset(p_message), myText, TEXT_BUFFER_SIZE + 1);
		myText[my_text_length] = '\0';
		printk("%s\n", myText);

		// if message type is confirmable send a acknowledgment
		if(messageType == OT_COAP_TYPE_CONFIRMABLE)
		{
			store_data_response_send(p_message, p_message_info);
		}

		
	}while(false);
}

static void store_data_response_send(otMessage *p_message, const otMessageInfo *p_message_info)
{	
	int error;
	otMessage *p_response;
	otInstance *p_instance = openthread_get_default_instance();

	p_response = otCoapNewMessage(p_instance, NULL);
	if(p_response==NULL){
		printk("Failed to allocate memory for CoAP response\n ");
		return;
	}

	do {
		error = otCoapMessageInitResponse(p_response, p_message, OT_COAP_TYPE_ACKNOWLEDGMENT, OT_COAP_CODE_CHANGED);
		if(error) break;

		error = otCoapSendResponse(p_instance, p_response, p_message_info);

	} while(false);

	if(error){
		printk("Failed to send store data response:%d\n", error);
		otMessageFree(p_response);
	}

}

static void get_data_request(void)
{
	int error = 0;
	otMessage *p_message;
	otMessageInfo p_message_info;
	otInstance *myInstance = openthread_get_default_instance();

	const otMeshLocalPrefix *ml_prefix = otThreadGetMeshLocalPrefix(myInstance);
	uint8_t serverInterfaceID[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x01};

	do{
		p_message = otCoapNewMessage(myInstance, NULL);
		if(p_message == NULL){
			printk("Failed to allocate memory for message");
			return;
		}

		otCoapMessageInit(p_message, OT_COAP_TYPE_NON_CONFIRMABLE, OT_COAP_CODE_GET);

		error = otCoapMessageAppendUriPathOptions(p_message, "getdata");
		if(error){printk("f1"); break;}

		error = otCoapMessageSetPayloadMarker(p_message);
		if(error){printk("f2"); break;}

		memset(&p_message_info, 0, sizeof(p_message_info));
		memcpy(&p_message_info.mPeerAddr.mFields.m8[0], ml_prefix, 8);
		memcpy(&p_message_info.mPeerAddr.mFields.m8[8], serverInterfaceID, 8);

		error = otCoapSendRequest(myInstance, p_message, &p_message_info, NULL, NULL);
		if(error) printk("f4");
	} while(false);

	if(error){
		printk("failed to send message: %d\n", error);
		otMessageFree(p_message);
	} else {
		printk("successfully sent a get request\n");
	}
}

void coap_init(void){
	
	int error;
	otInstance *p_instance = openthread_get_default_instance();

	do{
		error = otCoapStart(p_instance, OT_DEFAULT_COAP_PORT);
		if(error) break;

		otCoapAddResource(p_instance, &m_storedata_resource);
	}while(false);

	if(error){
		printk("Failed to initialize CoAP\n");
		return;
	}

	printk("successfully initialized CoAP\n");
}	

void addIPv6addr(void){

	otInstance *myInstance = openthread_get_default_instance();
	otNetifAddress aAddress;
	const otMeshLocalPrefix *ml_prefix = otThreadGetMeshLocalPrefix(myInstance);
	uint8_t interfaceID[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x01};

	memcpy(&aAddress.mAddress.mFields.m8[0], ml_prefix, 8);
	memcpy(&aAddress.mAddress.mFields.m8[8], interfaceID, 8);
}

void button_pressed_callback(const struct device *gpiob, struct gpio_callback *cb, gpio_port_pins_t pins){
	get_data_request();
}

void main(void)
{	
	
	addIPv6addr();
	coap_init();
	gpio_pin_configure_dt(&button0_spec, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button0_spec, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&button0_cb, button_pressed_callback, BIT(button0_spec.pin));
	gpio_add_callback(button0_spec.port, &button0_cb);

	while (1) {
		k_msleep(SLEEP_TIME_MS);
	}
}
