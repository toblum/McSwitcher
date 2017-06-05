#define MAX_SWITCHES 10          // Max. number of switches

#define AP_NAME "McSwitcher"     // Name of autoconnect AP / edit page
#define AP_PASS "SwitcherMC"     // Password for autoconnect AP / edit page

#define PIN_MODEBUTTON 0             // Data PIN for mode switch
#define PIN_RECEIVE D2           // Data PIN for receiver
#define PIN_TRANSMIT D1          // Data PIN for transmitter
#define NUM_TRANS_REPEATS 5      // Number of transmission repeats

struct switchstate               // Data structure to store a state of a switch
{
   byte state;
   int code_on;
   int code_off;
   String title;
};

typedef struct switchstate SwitchState;   // Define the datatype SwitchState
SwitchState switchstates[MAX_SWITCHES];   // Get an array of switch states to store the overall status
int num_switches = 0;                     // Currently active switches


// ***************************************************************************
// Helper
// ***************************************************************************
#define SERIALDEBUG true
#ifdef SERIALDEBUG
#define         DEBUG_PRINT(x)    Serial.print(x)
#define         DEBUG_PRINTLN(x)  Serial.println(x)
#define         DEBUG_PRINTF1(x)  Serial.printf(x)
#define         DEBUG_PRINTF2(x, y)  Serial.printf(x, y)
#define         DEBUG_PRINTF3(x, y, z)  Serial.printf(x, y, z)
#define         DEBUG_PRINTF4(x, y, z, a)  Serial.printf(x, y, z, a)
#else
#define         DEBUG_PRINT(x)
#define         DEBUG_PRINTLN(x)
#define         DEBUG_PRINTF1(x)
#define         DEBUG_PRINTF2(x, y)
#define         DEBUG_PRINTF3(x, y, z)
#define         DEBUG_PRINTF4(x, y, z, a)
#endif
