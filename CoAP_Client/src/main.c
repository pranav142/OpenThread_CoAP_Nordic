#include <zephyr/zephyr.h>
#include <zephyr/drivers/gpio.h>
#include <device.h>
#include <devicetree.h>
#include <net/openthread.h>
#include <openthread/thread.h>
#include <openthread/coap.h>
 
/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   100

/*
Configuring button specifications
*/
#define BUTTON0_NODE	DT_NODELABEL(button0)

static const struct gpio_dt_spec button0_spec = GPIO_DT_SPEC_GET(BUTTON0_NODE, gpios);
static struct gpio_callback button0_cb;

static void coap_send_data_request(void);
static void coap_send_data_response_cb(void *p_context, otMessage *p_message, const otMessageInfo *p_message_info, otError result);

static void store_get_request_cb(void *p_context, otMessage *p_message, const otMessageInfo *p_message_info);

#define TEXT_BUFFER_SIZE 30
char myText[TEXT_BUFFER_SIZE];
uint16_t my_text_length = 0;

static otCoapResource m_getdata_resource = {
	.mUriPath = "getdata",
	.mHandler = store_get_request_cb,
	.mContext = NULL,
	.mNext = NULL
};

static void store_get_request_cb(void *p_context, otMessage *p_message, const otMessageInfo *p_message_info)
{
	otCoapCode messageCode = otCoapMessageGetCode(p_message);
	otCoapType messageType = otCoapMessageGetType(p_message);
	uint16_t messageId = otCoapMessageGetMessageId(p_message);

	if (messageType == OT_COAP_TYPE_NON_CONFIRMABLE || messageType != OT_COAP_TYPE_CONFIRMABLE
		 && messageCode == OT_COAP_CODE_GET)
	{
		coap_send_data_request();
	}


}
void button_pressed_callback(const struct device*, struct gpio_callback*, gpio_port_pins_t);

static void coap_send_data_response_cb(void *p_context, otMessage *p_message, const otMessageInfo *p_message_info, otError result)
{
	if(result == OT_ERROR_NONE){
		printk("Delievery Confirmed\n");
	} else {
		printk("Delievery not confirmed: %d\n", result);
	}
}

static void coap_send_data_request(void)
{
	int error = 0;
	otMessage *myMessage;
	otMessageInfo myMessageInfo;
	otInstance *myInstance = openthread_get_default_instance();

	const otMeshLocalPrefix *ml_prefix = otThreadGetMeshLocalPrefix(myInstance);
	uint8_t serverInterfaceID[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x01};

	const char *myTemperatureJson = "{\"Temperature\":23.32}";

	do{
		myMessage = otCoapNewMessage(myInstance, NULL);
		if(myMessage == NULL){
			printk("Failed to allocate memory for message");
			return;
		}

		otCoapMessageInit(myMessage, OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_PUT);

		error = otCoapMessageAppendUriPathOptions(myMessage, "storedata");
		if(error){printk("f1"); break;}

		error = otCoapMessageSetPayloadMarker(myMessage);
		if(error){printk("f2"); break;}

		error = otMessageAppend(myMessage, myTemperatureJson, strlen(myTemperatureJson));
		if(error){printk("f3");break;}

		memset(&myMessageInfo, 0, sizeof(myMessageInfo));
		memcpy(&myMessageInfo.mPeerAddr.mFields.m8[0], ml_prefix, 8);
		memcpy(&myMessageInfo.mPeerAddr.mFields.m8[8], serverInterfaceID, 8);

		error = otCoapSendRequest(myInstance, myMessage, &myMessageInfo, coap_send_data_response_cb, NULL);
		if(error) printk("f4");
	} while(false);

	if(error){
		printk("failed to send message: %d\n", error);
		otMessageFree(myMessage);
	} else {
		printk("successfully sent the message\n");
	}

}

void button_pressed_callback(const struct device *gpiob, struct gpio_callback *cb, gpio_port_pins_t pins){
	coap_send_data_request();
}

void coap_init(void)
{	
	int error = 0;
	otInstance *myInstance = openthread_get_default_instance();

	do{

	error = otCoapStart(myInstance, OT_DEFAULT_COAP_PORT);
	if(error) break;

	otCoapAddResource(myInstance, &m_getdata_resource);

	} while(false);

	if (error) printk("failed to initialze coap %d\n", error);

}

void main(void)
{	
	coap_init();
	gpio_pin_configure_dt(&button0_spec, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button0_spec, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&button0_cb, button_pressed_callback, BIT(button0_spec.pin));
	gpio_add_callback(button0_spec.port, &button0_cb);

	while (1) {
		k_msleep(SLEEP_TIME_MS);
	}
}




