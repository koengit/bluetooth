## Briefly about BLE ##
Bluetooth (BT) is a wireless technology that specifies how data can be transmitted over short distances using 2.4 Mhz radio hardware. A BT connection can continuously stream quite large amounts of data. However, some devices only periodically need to transmit a very low amount of data, which makes the above description of BT unsuitable. This led to the development of Bluetooth Low Energy (BLE).

Battery life is increased by having BLE spend most of its time asleep, waiting for connections.

## How does BLE organise user data? ##
The Generic Attribute Profile (GATT) describes in detail how all user data is exchanged over a BLE connection. Conceptually related pieces of information, called characteristics, are organised by services. A biometrics service might expose two characteristics, your heart rate and your pulse. These values are defined as two distinct _characteristics_ which belong to the biometrics _service_.

Every piece of addressable information in a BLE device is called an attribute. Since both services and their characteristics are addressable pieces of information, they are both attributes (despite the characteristics 'belonging' to the service).

## How is data exchanged between BLE devices? ##
BLE devices must take either of two roles. (I have not yet checked if they can take both roles at the same time. Trying this should be quite straightforward.)
  + Server. A GATT server can declare services and other attributes. A server can not establish a connection to a client. All connections are initiated by GATT clients.
  + Client. A GATT client can connect to GATT servers and interact with the different attributes.

Characteristics can have many different permissions and configurations. The most interesting configurations are those that define whether the attribute can be read from, written to or subscribed to.  
When writing to an attribute a client must supply a buffer containing the data to be transmitted. In a sense, this is the same as sending a message to the server.  
The client can send a request to read a specific attribute, to which the server will respond with the value it has. It is important to emphasise here that **this flow of data from the server to the client is initiated by the client**.  
The client can choose to subscribe to an attribute on a server. The meaning of this is that a client will be made aware of any changes made to the value on the server. When a server modifies the value specified by an attribute it can choose to notify all subscribed clients of this change in one or two ways. **Notifying** a client just transmits a message with the notification, while **indicating** that a value has changed forces the client to confirm that the notification has been received (this is significantly slower).
When the server notifies subscribed clients of any changes, it transmits a buffer of data. A callback function on the client side is invoked when the notification is received, at which point the payload buffer can be inspected. **This flow of data from the server to the client is initiated by the server**.

When we use communication primitives such as `send(receiver, message)` and `receive(sender, message)` we talk about specific agents communicating. We are informed of who sent a message, and we specify who we wish to send a message to.  
Pilfering the notification machinery to implement the more general `send/receive` primitives will not work!
Client side:
  + Implementing send: send a write request to an attribute (a general attribute the server and client have agreed upon to use for communicating) on a server.
  + Implementing receive: Client subscribes to an agreed-upon attribute on a server and registers a callback to be invoked when notified of changes. As the payload sent alongside the notification can contain anything, this can contain a message for the client.
Server side:
  + Implementing receive: The server has a dedicated attribute that clients will write to, including their message payload in the write request. The attribute is essentially only a handle a client and a server agrees to communicate by.
  + Implementing send: **Not possible(?)** To send a message to a client (with the client initiating the exchange) the server would have to notify subscribers or a change to an attribute. However, many clients could have subscribed to said attribute, which would make the communication one-to-many.

## Description of the current Alexa example ##
On a remote GATT server two services are exposed, a temperature service and an Octavius service. The temperature service has one characteristic, the temperature, while the Octavius service similarly has one characteristic describing the permission to open the window.

Both the remote server and the Alexa (which is a GATT client) have agreed on a few pieces of information, namely the UUIDs of the device, the services the device exposes and the characteristics offered by the services.
```c
#define BT_UUID_DEVICE                             BT_UUID_DECLARE_16(0xffcc)

#define BT_UUID_TEMPERATURE_SENSOR_SERVICE         BT_UUID_DECLARE_16(0xff11)
#define BT_UUID_TEMPERATURE_SENSOR_CHARACTERISTIC  BT_UUID_DECLARE_16(0xff12)

#define BT_UUID_OCTAVIUS_SERVICE                   BT_UUID_DECLARE_16(0xff21)
#define BT_UUID_OCTAVIUS_CHARACTERISTIC            BT_UUID_DECLARE_16(0xff22)
```

When the GATT client, the Alexa, is started, it begins by synchronously activating the BLE hardware by calling `bt_enable(NULL);`. After this, it will install two callbacks which are invoked when connections are established or broken.

```c
static void connected(struct bt_conn *conn, u8_t conn_err){...}

static void disconnected(struct bt_conn *conn, u8_t reason){...}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

void main(void) {
    ...
    bt_conn_cb_register(&conn_callbacks);
    ...
}
```

At this point the BLE hardware is enabled and the machinery to handle connections is in place. The client will start scanning for available GATT servers.
```c
static void start_scan(void){
    struct bt_le_scan_param scan_param = {...};
    ...
    int err = bt_le_scan_start(&scan_param, device_found);
    ...
}
```
Device found is a function reference. This function will be called when a device has been found through scanning.
```c
static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t type, struct net_buf_simple *ad) {
    ...
    if (type == BT_GAP_ADV_TYPE_ADV_IND ||type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        bt_data_parse(ad, eir_found, (void *)addr);
    }
}
```
The method `device_found` will check the advertised data to see if the device that advertised its presence allows connections, and in that case begins parsing the advertised data. The parsing is handled by a built in API function `bt_data_parse`, and the result is passed to the callback `eir_found`.
```c
static bool eir_found(struct bt_data *data, void *user_data) {
    ...
    switch (data->type) {
    case BT_DATA_UUID16_SOME:
    case BT_DATA_UUID16_ALL:
    ...
        /* Loop through the advertised data. */
        for (i = 0; i < data->data_len; i += sizeof(u16_t)) {
            struct bt_le_conn_param *param;
            struct bt_uuid *uuid;
            u16_t u16;
            int err;

            /* We are looking for a specific device, the device that we know advertises the
            *  temperature and octavius services. We continue looping through the advertised
            *  data until we find the section that describes the device we looked for.
            * 
            *   We assume that the device exists. */
            memcpy(&u16, &data->data[i], sizeof(u16));
            uuid = BT_UUID_DECLARE_16(sys_le16_to_cpu(u16));
            if (bt_uuid_cmp(uuid, BT_UUID_DEVICE)) {
                continue;
            }

            /* We found the correct device, stop scanning. */
            err = bt_le_scan_stop();
            ...

            /* Call the BLE API to connect to the remote device */
            param = BT_LE_CONN_PARAM_DEFAULT;
            err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, param, &default_conn);
            ...

            return false;
        }
    }
    return true;
}
```
At this point we have initiated a connection and the callback we registered earlier is invoked. This callback is going to design two sets of service discovery parameters. One is configured to search for the temperature sensor service and the other for the octavius permission service. It knows what to look for by specifying which UUIDs the services are registered with on the server.
```c
static void connected(struct bt_conn *conn, u8_t conn_err) {
    ....
    if (conn == default_conn) {
        printk("Initiating scan for the octavius service\n");
        memcpy(&oct_uuid, BT_UUID_OCTAVIUS_SERVICE, sizeof(uuid));
        discover_oct_params.uuid = &oct_uuid.uuid;
        discover_oct_params.func = discover_octavius;
        discover_oct_params.start_handle = 0x0001;
        discover_oct_params.end_handle = 0xffff;
        discover_oct_params.type = BT_GATT_DISCOVER_PRIMARY;

        err = bt_gatt_discover(default_conn, &discover_oct_params);
        ...
        printk("Initiating scan for the temperature service\n");
        memcpy(&uuid, BT_UUID_TEMPERATURE_SENSOR_SERVICE, sizeof(uuid));
        discover_params.uuid = &uuid.uuid;
        discover_params.func = discover_temperature;
        discover_params.start_handle = 0x0001;
        discover_params.end_handle = 0xffff;
        discover_params.type = BT_GATT_DISCOVER_PRIMARY;

        err = bt_gatt_discover(default_conn, &discover_params);
        ...
    }
}
```
Notice here how the field `func` in the discovery parameter struct is different in the two sets of parameters. This is a function that will be called _if_ the service is found on the remote device. In both calls to `bt_gatt_discover` we use the same connection object (default conn). This is because both services reside on the same device right now.

Imagine that the octavius service has been found. This is the callback that is invoked.
```c
static u8_t discover_octavius(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params) {
    ...
    /* If we found the octavius service */
    if (!bt_uuid_cmp(discover_oct_params.uuid, BT_UUID_OCTAVIUS_SERVICE)) {
        /* Reconfigure the discovery parameters to look for the characteristic.
         * Again, we specify the characteristic we want with a UUID which the client and server
         * agreed to use. */
        memcpy(&oct_uuid, BT_UUID_OCTAVIUS_CHARACTERISTIC, sizeof(uuid));
        discover_oct_params.uuid = &oct_uuid.uuid;
        discover_oct_params.start_handle = attr->handle + 1;
        discover_oct_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &discover_oct_params);
        ...
    /* If we found the octavius characteristic, we end up here */
    } else if (!bt_uuid_cmp(discover_oct_params.uuid, BT_UUID_OCTAVIUS_CHARACTERISTIC)) {
        /* Again we reconfigure the discovery parameters, but this time to look for
         * a descriptor. I am not 100% sure what this is, I've not found any references
         * to it in the documentation. I imagine the characteristic being a header for a
         * descriptor that actually contains the value. This despite the fact that we
         * think of the characteristic as the value. I imagine the characteristic manages
         * information of the underlying value, its type etc. */
        memcpy(&oct_uuid, BT_UUID_GATT_CCC, sizeof(uuid));
        discover_oct_params.uuid = &oct_uuid.uuid;
        discover_oct_params.start_handle = attr->handle + 2;
        discover_oct_params.type = BT_GATT_DISCOVER_DESCRIPTOR;

        /* We prepare the subscription parameter object by indicating which
         * handle describes the attribute we wish to subscribe to.
         * Again, I can't find a lot of documentation concerning handles. But
         * each attribute is given a unique handle. So far this is all the information
         * I've needed to know about handles. */
        subscribe_oct_params.value_handle = bt_gatt_attr_value_handle(attr);

        err = bt_gatt_discover(conn, &discover_oct_params);
        ...
    /* We end up here when we found the last piece of information we want to scan for, the descriptor. */
    } else {
        /* We register a callback function notify_octavius which is invoked every time the
         * client is notified about a change of the octaviuss characteristic on the server side.*/
        subscribe_oct_params.notify = notify_octavius;
        subscribe_oct_params.value = BT_GATT_CCC_NOTIFY;
        subscribe_oct_params.ccc_handle = attr->handle;

        err = bt_gatt_subscribe(conn, &subscribe_oct_params);
        ...
    }

    return BT_GATT_ITER_STOP;
}
```
Given that the subcription succeeded, we are now subscribed and will be notified of changes to octavius value. The remote device will supply us with the status of octavius every second. The function that receives this message is this one.
```c
static u8_t notify_octavius(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data, u16_t length) {
    ...
    /* Message from temp/octavius */
    int* input = (int*) data;
    ...

    /***** Prepare to call Nachi's function *****/
    func(/* Some dummy skip value */, *input);
    /********************************************/

    return BT_GATT_ITER_CONTINUE;
}
```