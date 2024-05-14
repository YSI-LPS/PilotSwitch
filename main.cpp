#include <string>
#include <sstream>
#include "lib_Transmission.h"   // si les NAN ne fonctionne pas aller dans mbed-os/tools/profiles/ puis ARMC6 et dans common ajouter -fhonor-nans
#include "kvstore_global_api.h"

#define MBED_PROJECT    "PilotSwitch"

BusOut              Led(LED1, LED2, LED3);
UnbufferedSerial    pc(USBTX, USBRX, 230400);
EthernetInterface   eth;
Thread              *queueThread = NULL;
EventQueue          queue;

enum enumHTML       {   CONTENT, HEAD, HEAD_META, OPEN_BODY, OPEN_BODY_IFRAME, CLOSE_BODY, FAVICON, ERR404, SMTP };
enum enumMODE       {   THRESHOLD, HYSTERESIS, RANGE };
enum enumPARAM      {   MAC, NAME, NETWORK, SPEC };
enum enumSETTING    {   LOWER, UPPER, PERIOD, MODE, NOTIFY, NB_SETTINGS };
enum enumSENSOR     {   BATTERY, SECTOR, PRESSURE, THERMISTANCE, THERMOCOUPLE, SHT25_TEMP_I2C0, SHT25_HUM_I2C0, SHT25_TEMP_I2C1, SHT25_HUM_I2C1, NB_SENSORS };

struct  {   Transmission::enum_trans_status state;
            const Transmission::enum_trans_status client;
            bool dhcp;
            uint16_t port;
            string ip;
            string mask;
            string gateway;
            int color[8];
} connection = { Transmission::RED_DISCONNECTED, Transmission::BLUE_CLIENT, false, 80, "192.168.1.25", "255.255.255.0", "192.168.1.0",
#if(defined(TARGET_LPC1768) || defined(TARGET_K64F))
    { 0, 1, 2, 3, 4, 5, 6, 7 }};
#elif(defined(TARGET_NUCLEO_H743ZI2) || defined(TARGET_NUCLEO_F767ZI) || defined(TARGET_NUCLEO_F429ZI) || defined(TARGET_NUCLEO_F746ZG) || defined(TARGET_NUCLEO_F756ZG))
    { 7, 6, 3, 2, 5, 1, 4, 0 }};
#endif

struct {    string alerts;
            string pwd;
            string adm;
            string mail[5];
            string param[4];
} uc = { "", { 0x2A, 0x2A, 0x55, 0x43 },  { 0x2A, 0x45, 0x4C, 0x49, 0x4E, 0x53, 0x59, 0x53, 0x49, 0x2A }, { "YANNIC.SIMON@UNIVERSITE-PARIS-SACLAY.FR", "", "", "", "" }, { "", "", "", "" } };

/* INITIALIZE */
void                setup(void);

/* TRANSMISSION */
void                ethset(string = "");
void                ethup(void);
string              processing(string);
Transmission        transmission(&pc, &eth, &processing, &ethup);
const string        MBED_IDN(MBED_PROJECT + ((string)" Rev. ") + MBED_STRINGIFY(TARGET_NAME) + " " + __DATE__ + " " + __TIME__ + " MbedOS " + to_string(MBED_VERSION));

/* HTML */
string              html(const enumHTML&, const enumSENSOR& = NB_SENSORS, const bool& = false);

/* KVSTORE */
string              kv(string, string = "");
void                kv_apply(string, string = "");

int main(void)
{
    while(1) ThisThread::sleep_for(200ms);
}

void setup(void)
{
    Led = connection.color[Transmission::WHITE_STATUS];
    ThisThread::sleep_for(500ms);
    Led = connection.color[connection.state];
    kv_apply("IDN");
    kv_apply("ETH");
    queueThread = new Thread;
    queueThread->start(callback(&queue, &EventQueue::dispatch_forever));
}

void ethup(void)
{
    
}

string kv(string kvKey, string kvParam)
{
    if(kvKey.empty()) return "";
    size_t keySize;
    kv_info_t keyInfo;
    char kvValue[256] = {0};
    if((kvKey == "IDN") && !kvParam.empty()) kv_reset("/kv/");
    if(kv_get_info("IDN", &keyInfo) != MBED_SUCCESS) if(kv_reset("/kv/") == MBED_SUCCESS) kv_set("IDN", "", 0, 0);
    if(kv_get_info(kvKey.c_str(), &keyInfo) == MBED_SUCCESS) if(kv_get(kvKey.c_str(), kvValue, 256, &keySize) == MBED_SUCCESS) if(!kvParam.empty()) kv_remove(kvKey.c_str());
    if(!kvParam.empty()) kv_set(kvKey.c_str(), kvParam.c_str(), kvParam.size(), 0);
    return kvValue;
}

void kv_apply(string kvKey, string kvParam)
{
    string kvValue = kv(kvKey);
    enum enumKvSTATUS { KVAL_PVAL, NOTKVAL_PVAL, KVAL_NOTPVAL, NOTKVAL_NOTPVAL } kvStatus = enumKvSTATUS((1*kvValue.empty()) + (2*kvParam.empty()));
    if(kvKey == "IDN")
    {
        if(kvValue != MBED_IDN) kv(kvKey, MBED_IDN);
    }
    else if(kvKey == "ETH")
    {
        processing((kvValue.substr(0, 3) == "ETH")?kvValue:"ETH=DHCP");
    }
    else
    {
        
    }
}

void ethset(string cmd)
{
    size_t pos;
    if((connection.dhcp = cmd.empty())) cmd = transmission.ip(transmission.ip());
    if((pos = cmd.find(":")) != string::npos)
    {
        istringstream istr(cmd.substr(pos+1));
        istr >> connection.port;
        cmd.replace(pos, 1, " ");
    }
    istringstream sget(cmd);
    for(int dot = 0, data = 0; getline(sget, cmd, ' '); dot = 0)
    {
        for(const char c:cmd) if(c == '.') dot++;
        if(dot == 3)
        {
            data++;
            if(cmd == "0.0.0.0") break;
            else
            {
                istringstream istr(cmd);
                switch(data)
                {
                    case 1: istr >> connection.ip;      break;
                    case 2: istr >> connection.mask;    break;
                    case 3: istr >> connection.gateway; break;
                    default:                            break;
                }
            }
        }
    }
    if((cmd == "0.0.0.0") && connection.dhcp) queue.call_in(10s, &ethset, "");
}

string html(const enumHTML& PART, const enumSENSOR& SENSORS, const bool& BACKUP)
{
    ostringstream ssend;
    ssend << fixed;
    ssend.precision(2);
    switch(PART)
    {
        case CONTENT:           ssend << "Content-Type: text/html; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n\r\n"; break;
        case HEAD:              ssend << "<!DOCTYPE html>\r\n<html>\r\n\t<head>\r\n\t\t<title>" << MBED_PROJECT << "</title>\r\n\t</head>"; break;
        case HEAD_META:         ssend << "<!DOCTYPE html>\r\n<html>\r\n\t<head>\r\n\t\t<title>" << MBED_PROJECT << "</title>\r\n\t\t<meta http-equiv=refresh content=10>\r\n\t</head>"; break;
        case OPEN_BODY:         ssend << "\r\n\t<body style=background-color:dimgray>\r\n\t\t<center>\r\n\t\t\t<h1 style=color:white>" << MBED_PROJECT << "</h1>"; break;
        case OPEN_BODY_IFRAME:  ssend << "\r\n\t<body style=background-color:transparency>\r\n\t\t<center>"; break;
        case CLOSE_BODY:        ssend << "\r\n\t\t</center>\r\n\t</body>\r\n</html>"; break;
        case FAVICON:
            ssend << "Content-Type: image/svg+xml\r\nAccess-Control-Allow-Origin: *\r\n\r\n<svg width=\"100%\" height=\"100%\" viewBox=\"0 0 100 100\" style=\"background-color:black\" xmlns=\"http://www.w3.org/2000/svg\">\r\n\t<title>" << MBED_PROJECT;
            ssend << "</title>\r\n\t<text x=\"0\" y=\"90\" textLength=\"80\" font-size=\"120\" fill=\"white\" font-weight=\"bold\" font-style=\"italic\" lengthAdjust=\"spacingAndGlyphs\" style=\"font-family:\'Bauhaus 93\';-inkscape-font-specification:\'Bauhaus 93, Normal\'\">ElI</text>\r\n</svg>";
        break;
        case ERR404:
            ssend << "Content-Type: image/svg+xml\r\nAccess-Control-Allow-Origin: *\r\n\r\n<svg width=\"100%\" height=\"100%\" viewBox=\"0 0 100 100\" style=\"background-color:black\" xmlns=\"http://www.w3.org/2000/svg\">\r\n\t<title>" << MBED_PROJECT;
            ssend << "</title>\r\n\t<text x=\"0\" y=\"90\" textLength=\"80\" font-size=\"120\" fill=\"white\" font-weight=\"bold\" font-style=\"italic\" lengthAdjust=\"spacingAndGlyphs\" style=\"font-family:\'Bauhaus 93\';-inkscape-font-specification:\'Bauhaus 93, Normal\'\">404</text>\r\n</svg>";
        break;
        case SMTP:
        break;
    }
    return ssend.str();
}

string processing(string cmd)
{
    ostringstream ssend;
    ssend << fixed;
    ssend.precision(2);
    if(cmd.empty());
    else if(cmd == "*IDN?")
        return MBED_IDN;
    else if(cmd == "MAC?")
        ssend << "MAC\t[" << (uc.param[MAC].empty()?(eth.get_mac_address()?eth.get_mac_address():"00:00:00:00:00:00"):uc.param[MAC]) << "]";
    else if(cmd == "CLIENT?")
        ssend << "CLIENT\t[" << transmission.client() << "]";
    else if(cmd == "ETH?")
        ssend << "ETH\t[" << (connection.dhcp?"DHCP":"STATIC") << ((eth.get_connection_status() == NSAPI_STATUS_GLOBAL_UP)?":UP]":":DOWN]");
    else if(cmd == "IP?")
        ssend << "IP\t[" << transmission.ip() << "]";
    else if(cmd == "IP=ALL?")
        ssend << "IP=ALL\t[" << transmission.ip(transmission.ip()) << "]";
    else if(cmd == "IP=MEM?")
        ssend << "IP=MEM\t[" << connection.ip << ":" << connection.port << " " << connection.mask << " " << connection.gateway << "]";
    else if(cmd == "ETH=OFF")
        uc.param[MAC] = transmission.ip(false);
    else if(cmd == "ETH=REBOOT")
    {
        queue.call(&processing, "ETH=OFF");
        queue.call(&kv_apply, "ETH", "");
        // queue.call_in(30s, &notify, "REBOOT", "", NB_SENSORS, "");
    }
    else if(cmd == "ETH=DHCP")
    {
        queue.call(&ethset, "");
        uc.param[MAC] = transmission.ip(true);
        kv("ETH", "ETH=DHCP");
    }
    else if(cmd.find("ETH=IP") != string::npos)
    {
        ethset(cmd);
        uc.param[MAC] = transmission.ip(true, connection.ip.c_str(), connection.port, connection.mask.c_str(), connection.gateway.c_str());
        kv("ETH", "ETH=IP " + connection.ip + ":" + to_string(connection.port) + " " + connection.mask + " " + connection.gateway);
    }
    else if(cmd.find("HEAD /") != string::npos)
        ssend << transmission.http.RETURN_OK << html(CONTENT);
    else if(cmd.find("GET /FAVICON.ICO HTTP") != string::npos)
        ssend << transmission.http.RETURN_OK << html(FAVICON);
    else if(cmd.find("GET / HTTP") != string::npos)
    {
        ssend << transmission.http.RETURN_OK << html(CONTENT) << html(HEAD_META) << html(OPEN_BODY);
        ssend << "\r\n\t\t\t<table style=border-collapse:collapse>";
        // for(const typeSensor &s : sensor) if(s.status != "MISSING") ssend << "\r\n\t\t\t\t<tr style=background-color:" << BgColor[s.status] << "><th style=text-align:left;padding:5px>" << s.name << "</th><td style=text-align:center;padding:5px>" << (s.unity.empty()?s.status:str_precision(s.value)) << s.unity << "</td></tr>";
        ssend << "\r\n\t\t\t</table><br>\r\n\t\t\t<form method=post>\r\n\t\t\t\t<a href=/history><button type=button style=text-align:center;width:5em>History</button></a>\r\n\t\t\t\t<button type=submit formaction=/settings>Settings</button>\r\n\t\t\t</form>";
        ssend << html(CLOSE_BODY);
    }
    else if(cmd.find("GET /*IDN? HTTP") != string::npos)
        ssend << transmission.http.RETURN_OK << html(CONTENT) << MBED_IDN;
    else if(cmd.find("GET /") != string::npos)
        ssend << transmission.http.RETURN_NOT_FOUND << html(ERR404);
    else if(cmd[cmd.size()-1] == '?')
        ssend << "incorrect requeste [" << cmd << "]";
    return ssend.str();
}