 //<>//
//////////////////////////////////////////////////////////////////////////////////////////
//
//   Desktop GUI for controlling the HealthyPi HAT [ Patient Monitoring System]
//
//   Copyright (c) 2016 ProtoCentral
//   
//   This software is licensed under the MIT License(http://opensource.org/licenses/MIT). 
//   
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT 
//   NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
//   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
//   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
//   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//   Requires g4p_control graphing library for processing.  Built on V4.1
//   Downloaded from Processing IDE Sketch->Import Library->Add Library->G4P Install
//
/////////////////////////////////////////////////////////////////////////////////////////

import g4p_controls.*;                       // Processing GUI Library to create buttons, dropdown,etc.,
import processing.serial.*;                  // Serial Library
import grafica.*;
import processing.net.*;
import mqtt.*;

MQTTClient mqttclient;

Client myClient; 
int dataIn; 

// Java Swing Package For prompting message
import java.awt.*;
import javax.swing.*;
import static javax.swing.JOptionPane.*;

// File Packages to record the data into a text file
import javax.swing.JFileChooser;
import java.io.FileWriter;
import java.io.BufferedWriter;

// Date Format
import java.util.Date;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.net.UnknownHostException;

// General Java Package
import java.math.*;

import controlP5.*;

/************** Packet Validation  **********************/
private static final int CESState_Init = 0;
private static final int CESState_SOF1_Found = 1;
private static final int CESState_SOF2_Found = 2;
private static final int CESState_PktLen_Found = 3;

/*CES CMD IF Packet Format*/
private static final int CES_CMDIF_PKT_START_1 = 0x0A;
private static final int CES_CMDIF_PKT_START_2 = 0xFA;
private static final int CES_CMDIF_PKT_STOP = 0x0B;
private static final int CES_CMDIF_PKT_PKTTYPE = 0x03;
private static final int ECG_SAMPLES_PER_PKT = 8;

/*CES CMD IF Packet Indices*/
private static final int CES_CMDIF_IND_LEN = 2;
private static final int CES_CMDIF_IND_LEN_MSB = 3;
private static final int CES_CMDIF_IND_PKTTYPE = 4;
private static int CES_CMDIF_PKT_OVERHEAD = 5;

/************** Packet Related Variables **********************/

int ecs_rx_state = 0;                                        // To check the state of the packet
int CES_Pkt_Len;                                             // To store the Packet Length Deatils
int CES_Pkt_Pos_Counter, CES_Data_Counter;                   // Packet and data counter
int CES_Pkt_PktType;                                         // To store the Packet Type

char CES_Pkt_Data_Counter[] = new char[1000];                // Buffer to store the data from the packet
char ces_pkt_temp_bytes[] = new char[4];                     // Buffer to hold data

int pSize = 1500;                                            // Total Size of the buffer
int tcgpSize=60;

int arrayIndex = 0;                                          // Increment Variable for the buffer
int tcgArrayIndex=0;

float time = 0;                                              // X axis increment variable

// Buffer for ecg,spo2,respiration,and average of thos values
float[] ecgdata = new float[pSize];   
float[] rtorArray = new float[pSize];

/************** Graph Related Variables **********************/

double maxe, mine, maxr, minr, maxs, mins;             // To Calculate the Minimum and Maximum of the Buffer
double ecg, rtor_value, hr;  // To store the current ecg value
double respirationVoltage=20;                          // To store the current respiration value
boolean startPlot = false;                             // Conditional Variable to start and stop the plot
double prev_rtor_value=0;

GPlot plotECG;
GPlot plotTCG;

int step = 0;
int stepsPerCycle = 100;
int lastStepTime = 0;
boolean clockwise = true;
float scale = 5;

/************** File Related Variables **********************/

boolean logging = false;                                // Variable to check whether to record the data or not
FileWriter output;                                      // In-built writer class object to write the data to file
JFileChooser jFileChooser;                              // Helps to choose particular folder to save the file
Date date;                                              // Variables to record the date related values                              
BufferedWriter bufferedWriter;
DateFormat dateFormat;

/************** Port Related Variables **********************/

Serial port = null;                                     // Oject for communicating via serial port
String[] comList;                                       // Buffer that holds the serial ports that are paired to the laptop
char inString = '\0';                                   // To receive the bytes from the packet
String selectedPort;                                    // Holds the selected port number

/************** Logo Related Variables **********************/

PImage logo;
boolean gStatus;                                        // Boolean variable to save the grid visibility status

int nPoints1 = pSize;

int nPointsTCG=tcgpSize;

int totalPlotsHeight=0;
int totalPlotsWidth=0;

ControlP5 cp5;

Textlabel lblHR;
Textlabel lblRTOR;


public void setup() 
{
  println(System.getProperty("os.name"));
  
  GPointsArray pointsECG = new GPointsArray(nPoints1);
  GPointsArray pointsTCG = new GPointsArray(nPointsTCG);

  size(800, 600, JAVA2D);
  //fullScreen();
  /* G4P created Methods */
  //createGUI();
  //customGUI();
  
  totalPlotsHeight=height-50-68-15;

  makeGUI();
  plotECG = new GPlot(this);
  plotECG.setPos(0,170);
  plotECG.setDim(width, (totalPlotsHeight*0.5)-10);
  plotECG.setBgColor(0);
  plotECG.setBoxBgColor(0);
  plotECG.setLineColor(color(0, 255, 0));
  plotECG.setLineWidth(3);
  plotECG.setMar(0,0,0,0);
  
  plotTCG = new GPlot(this);
  plotTCG.setPos(0,(170+(totalPlotsHeight*0.5)-10));
  plotTCG.setDim(width, (totalPlotsHeight*0.5)-10);
  plotTCG.setBgColor(0);
  plotTCG.setBoxBgColor(0);
  plotTCG.setLineColor(color(0, 0, 255));
  plotTCG.setLineWidth(3);
  plotTCG.setMar(0,0,0,0);
  
  for (int i = 0; i < nPoints1; i++) 
  {
    pointsECG.add(i,0);
  }
  
    for (int i = 0; i < nPointsTCG; i++) 
  {
    pointsTCG.add(i,0);
  }

  plotECG.setPoints(pointsECG);
  plotTCG.setPoints(pointsTCG);
  
  /*******  Initializing zero for buffer ****************/

  for (int i=0; i<pSize; i++) 
  {
    ecgdata[i] = 0;
  }
  time = 0;
  
    //myClient = new Client(this, "heartypatch.local", 4567);
    //myClient = new Client(this, "192.168.1.117", 4567);
    //startPlot = true;
    
}

void messageReceived(String topic, byte[] payload) {
  println("new message: " + topic + " - " + new String(payload));
}

void clientEvent(Client someClient) 
{
  /*
  print("Server Says:  ");
  dataIn = myClient.read();
  println(dataIn);
  background(dataIn);
  */
  inString = myClient.readChar();
  pc_processData(inString);

}

public void draw() 
{
  background(0);
  GPointsArray pointsECG = new GPointsArray(nPoints1);
  GPointsArray pointsTCG = new GPointsArray(nPointsTCG);
 
  if (startPlot)                             // If the condition is true, then the plotting is done
  {
    for(int i=0; i<nPoints1;i++)
    {    
      pointsECG.add(i,ecgdata[i]);
    }
    
    for(int i=0; i<nPointsTCG;i++)
    {    
      pointsTCG.add(i,rtorArray[i]);
    }
  } 

  plotECG.setPoints(pointsECG);
  
  plotECG.beginDraw();
  plotECG.drawBackground();
  plotECG.drawLines();
  plotECG.endDraw();
  
  plotTCG.setPoints(pointsTCG);
  
  plotTCG.beginDraw();
  plotTCG.drawBackground();
  plotTCG.drawLines();
  plotTCG.endDraw();
  
  pushStyle();
  noStroke();
  fill(255, 255, 255);
  rect(0, 0, width, 60);
  popStyle();
}

/*********************************************** Opening Port Function ******************************************* **************/

void startTCP()
{
    String devaddress = cp5.get(Textfield.class,"Address").getText();
   
    myClient = new Client(this, devaddress, 4567);
 
    if (true==myClient.active())
    {
        showMessageDialog(null, "Connected to "+devaddress, "Connected", INFORMATION_MESSAGE);
        startPlot = true;  
    }
    else
    {
        showMessageDialog(null, "Cannot connect to "+devaddress, "Alert", ERROR_MESSAGE);

    }
}
void startSerial()
{
  try
  {
    port = new Serial(this, selectedPort, 115200);
    port.clear();
    startPlot = true;
  }
  catch(Exception e)
  {

    showMessageDialog(null, "Port is busy", "Alert", ERROR_MESSAGE);
    System.exit (0);
  }
}

public void makeGUI()
{

  cp5 = new ControlP5(this);
   cp5.addButton("Connect")
     .setValue(0)
     .setPosition(310,10)
     .setSize(100,40)
     .setFont(createFont("Impact",15))
     .addCallback(new CallbackListener() {
      public void controlEvent(CallbackEvent event) {
        if (event.getAction() == ControlP5.ACTION_RELEASED) 
        {
          startTCP();
          //cp5.remove(event.getController().getName());
        }
      }
     } 
     );
     
   cp5 = new ControlP5(this);
   cp5.addButton("Close")
     .setValue(0)
     .setPosition(width-110,10)
     .setSize(100,40)
     .setFont(createFont("Impact",15))
     .addCallback(new CallbackListener() {
      public void controlEvent(CallbackEvent event) {
        if (event.getAction() == ControlP5.ACTION_RELEASED) 
        {
          CloseApp();
          //cp5.remove(event.getController().getName());
        }
      }
     } 
     );
  
   cp5.addButton("Record")
     .setValue(0)
     .setPosition(width-225,10)
     .setSize(100,40)
     .setFont(createFont("Impact",15))
     .addCallback(new CallbackListener() {
      public void controlEvent(CallbackEvent event) {
        if (event.getAction() == ControlP5.ACTION_RELEASED) 
        {
          RecordData();
          //cp5.remove(event.getController().getName());
        }
      }
     } 
     );
     
     lblRTOR = cp5.addTextlabel("lblSRTOR")
      .setText("R-R I: --- msec")
      .setPosition(width-200,100)
      .setColorValue(color(255,255,255))
      .setFont(createFont("Impact",20));
      
     lblHR = cp5.addTextlabel("lblHR")
      .setText("HR: --- bpm")
      .setPosition(width-200,300)
      .setColorValue(color(255,255,255))
      .setFont(createFont("Impact",20));
      
       cp5.addTextfield("Address")
           .setPosition(10,10)
           .setSize(200,25)
           //.setFont(font)
           .setFont(createFont("Impact",15))
           .setFocus(true)
           .setText("heartypatch.local")
           .setColorLabel(color(0,0,0))
           .setColor(color(255,255,255));
}

public void RecordData()
{
    try
  {
    jFileChooser = new JFileChooser();
    jFileChooser.setSelectedFile(new File("log.csv"));
    jFileChooser.showSaveDialog(null);
    String filePath = jFileChooser.getSelectedFile()+"";

    if ((filePath.equals("log.txt"))||(filePath.equals("null")))
    {
    } else
    {    
      logging = true;
      date = new Date();
      output = new FileWriter(jFileChooser.getSelectedFile(), true);
      bufferedWriter = new BufferedWriter(output);
      bufferedWriter.write(date.toString()+"");
      bufferedWriter.newLine();
      bufferedWriter.write("PktSeqID,PktTimeStamp,ECG,RTOR,HR");
      bufferedWriter.newLine();
    }
  }
  catch(Exception e)
  {
    println("File Not Found");
  }
}

public void CloseApp() 
{
  int dialogResult = JOptionPane.showConfirmDialog (null, "Would You Like to Close The Application?");
  if (dialogResult == JOptionPane.YES_OPTION) {
    try
    {
      //Runtime runtime = Runtime.getRuntime();
      //Process proc = runtime.exec("sudo shutdown -h now");
      System.exit(0);
    }
    catch(Exception e)
    {
      exit();
    }
  } else
  {
  }
}


/*********************************************** Serial Port Event Function *********************************************************/

///////////////////////////////////////////////////////////////////
//
//  Event Handler To Read the packets received from the Device
//
//////////////////////////////////////////////////////////////////

void serialEvent (Serial blePort) 
{
  inString = blePort.readChar();
  pc_processData(inString);
}

/*********************************************** Getting Packet Data Function *********************************************************/

///////////////////////////////////////////////////////////////////////////
//  
//  The Logic for the below function :
//      //  The Packet recieved is separated into header, footer and the data
//      //  If Packet is not received fully, then that packet is dropped
//      //  The data separated from the packet is assigned to the buffer
//      //  If Record option is true, then the values are stored in the text file
//
//////////////////////////////////////////////////////////////////////////

void pc_processData(char rxch)
{
  switch(ecs_rx_state)
  {
  case CESState_Init:
    if (rxch==CES_CMDIF_PKT_START_1)
      ecs_rx_state=CESState_SOF1_Found;
    break;

  case CESState_SOF1_Found:
    if (rxch==CES_CMDIF_PKT_START_2)
      ecs_rx_state=CESState_SOF2_Found;
    else
      ecs_rx_state=CESState_Init;                    //Invalid Packet, reset state to init
    break;

  case CESState_SOF2_Found:
    ecs_rx_state = CESState_PktLen_Found;
    CES_Pkt_Len = (int) rxch;
    CES_Pkt_Pos_Counter = CES_CMDIF_IND_LEN;
    CES_Data_Counter = 0;
    break;

  case CESState_PktLen_Found:
    CES_Pkt_Pos_Counter++;
    if (CES_Pkt_Pos_Counter < CES_CMDIF_PKT_OVERHEAD)  //Read Header
    {
      if (CES_Pkt_Pos_Counter==CES_CMDIF_IND_LEN_MSB)
        CES_Pkt_Len = (int) ((rxch<<8)|CES_Pkt_Len);
      else if (CES_Pkt_Pos_Counter==CES_CMDIF_IND_PKTTYPE)
        CES_Pkt_PktType = (int) rxch;
    } else if ( (CES_Pkt_Pos_Counter >= CES_CMDIF_PKT_OVERHEAD) && (CES_Pkt_Pos_Counter < CES_CMDIF_PKT_OVERHEAD+CES_Pkt_Len+1) )  //Read Data
    {
      if (CES_Pkt_PktType == CES_CMDIF_PKT_PKTTYPE)
      {
        CES_Pkt_Data_Counter[CES_Data_Counter++] = (char) (rxch);          // Buffer that assigns the data separated from the packet
      }
    } else  //All  and data received
    {
      if (rxch==CES_CMDIF_PKT_STOP)
      { 
        long seq = bytesToUnsigned(0, 4);
        long timestamp_sec = bytesToUnsigned(4, 4);
        long timestamp_usec = bytesToUnsigned(8, 4);
        float timestamp = timestamp_sec + .000001 * timestamp_usec;
        
        rtor_value = (double)bytesToUnsigned(12, 4);
        if (rtor_value != 0) 
            hr = 60*1000.0 / rtor_value;
        else
            hr = 0.0;

        for (int i=0; i<ECG_SAMPLES_PER_PKT; i++) {
            ecg = (double)bytesToInt(16+4*i, 4) /64;
            if (logging == true)
            {
              try {
                date = new Date();
                dateFormat = new SimpleDateFormat("HH:mm:ss");
                bufferedWriter.write(seq+","+String.format("%.3f", timestamp)+","+ecg+","+String.format("%.0f", rtor_value)+","+String.format("%.1f", hr));
                bufferedWriter.newLine();
              }
              catch(IOException e) {
                println("It broke!!!");
                e.printStackTrace();
              }
            }
            
            // Assigning the ECG value for the graph buffer
            ecgdata[arrayIndex++] = (float)ecg;
            if (arrayIndex == pSize)
                arrayIndex = 0;
        }
        
        
        // Assigning the RTOR value for the graph buffer
        textSize(24);
        lblRTOR.setText("R-R:"+String.format("%.0f", rtor_value)+" ms");
        lblHR.setText("HR: "+String.format("%.0f", hr)+" bpm");
        
        if(rtor_value!=prev_rtor_value)
        {
          //new sample
          rtorArray[tcgArrayIndex]=(float) rtor_value;
          tcgArrayIndex++;      
        }
        
        prev_rtor_value=rtor_value;
        
         if (tcgArrayIndex == tcgpSize)
        {  
          tcgArrayIndex = 0;
        } 
               
        ecs_rx_state=CESState_Init;
      } else
      {
        ecs_rx_state=CESState_Init;
      }
    }
    break;

  default:
    break;
  }
}


/*********************************************** Functions To Convert Data *********************************************************/

public int bytesToInt(int offset, int n)
{
  // copy data
  for (int i=0; i<n; i++) {
    ces_pkt_temp_bytes[i] = CES_Pkt_Data_Counter[offset+i];
  }
    
   return pc_AssembleBytes(ces_pkt_temp_bytes, n-1);
}


public long bytesToUnsigned(int offset, int n)
{
  // copy data
  for (int i=0; i<n; i++) {
    ces_pkt_temp_bytes[i] = CES_Pkt_Data_Counter[offset+i];
  }
    
   return pc_AssembleBytesUnsigned(ces_pkt_temp_bytes, n-1);
}
/*********************************************** Recursion Function To Reverse The data *********************************************************/

public int pc_AssembleBytes(char DataRcvPacket[], int n)
{
  if (n == 0)
    return (int) DataRcvPacket[n]<<(n*8);
  else
    return (DataRcvPacket[n]<<(n*8))| pc_AssembleBytes(DataRcvPacket, n-1);
}

public long pc_AssembleBytesUnsigned(char DataRcvPacket[], int n)
{
  if (n == 0)
    return (int) DataRcvPacket[n]<<(n*8);
  else
    return (DataRcvPacket[n]<<(n*8))| pc_AssembleBytesUnsigned(DataRcvPacket, n-1);
}

/********************************************* User-defined Method for G4P Controls  **********************************************************/

///////////////////////////////////////////////////////////////////////////////
//
//  Customization of controls is done here
//  That includes : Font Size, Visibility, Enable/Disable, ColorScheme, etc.,
//
//////////////////////////////////////////////////////////////////////////////