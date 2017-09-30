//////////////////////////////////////////////////////////////////////////////////////////
//
//   Raspberry Pi/ Desktop GUI for controlling the HealthyPi HAT v3
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
/////////////////////////////////////////////////////////////////////////////////////////

import g4p_controls.*;                       // Processing GUI Library to create buttons, dropdown,etc.,
import processing.serial.*;                  // Serial Library
import grafica.*;

// Java Swing Package For prompting message
import java.awt.*;
import javax.swing.*;
import static javax.swing.JOptionPane.*;

// File Packages to record the data into a text file
import javax.swing.JFileChooser;
import java.io.FileWriter;
import java.io.BufferedWriter;

// Date Format
import java.util.*;
import java.text.DateFormat;
import java.text.SimpleDateFormat;

// General Java Package
import java.math.*;
import controlP5.*;
//import http.requests.*;

ControlP5 cp5;

Textlabel lblTCG;
Textlabel lblSPO2;
Textlabel lblRR;
Textlabel lblBP;
Textlabel lblTemp;
Textlabel lblRRI;

Textlabel lblArrStatus;

/************** Packet Validation  **********************/
private static final int CESState_Init = 0;
private static final int CESState_SOF1_Found = 1;
private static final int CESState_SOF2_Found = 2;
private static final int CESState_PktLen_Found = 3;

/*CES CMD IF Packet Format*/
private static final int CES_CMDIF_PKT_START_1 = 0x0A;
private static final int CES_CMDIF_PKT_START_2 = 0xFA;
private static final int CES_CMDIF_PKT_STOP = 0x0B;

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
char CES_Pkt_ECG_Counter[] = new char[4];                    // Buffer to hold ECG data
char CES_Pkt_Resp_Counter[] = new char[4];                   // Respiration Buffer
char CES_Pkt_SpO2_Counter_RED[] = new char[4];               // Buffer for SpO2 RED
char CES_Pkt_SpO2_Counter_IR[] = new char[4];                // Buffer for SpO2 IR
int pSize = 60;                                            // Total Size of the buffer
int arrayIndex = 0;                                          // Increment Variable for the buffer
float time = 0;                                              // X axis increment variable

// Buffer for ecg,spo2,respiration,and average of thos values
float[] xdata = new float[pSize];
float[] ecgdata = new float[pSize];

float[] histdata = new float[30];

float[] bpmArray = new float[pSize];

/************** Graph Related Variables **********************/

double maxe, mine, maxr, minr, maxs, mins;             // To Calculate the Minimum and Maximum of the Buffer
double ecg, resp, spo2_ir, spo2_red, spo2, redAvg, irAvg, ecgAvg, resAvg;  // To store the current ecg value
double respirationVoltage=20;                          // To store the current respiration value
boolean startPlot = false;                             // Conditional Variable to start and stop the plot

GPlot plotPoincare;
GPlot plotTach;
GPlot plotdRRHist;

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
int totalPlotsHeight=0;
int totalPlotsWidth=0;
int heightHeader=50;
int updateCounter=0;

boolean is_raspberrypi=false;

int global_hr;
int global_rr;
float global_temp;
int global_spo2;

int global_test=0;

boolean ECG_leadOff,spo2_leadOff;
boolean ShowWarning = true;
boolean ShowWaringSpo2=true;

public void setup() 
{
  println(System.getProperty("os.name"));
  println(System.getProperty("os.arch"));
  
  GPointsArray pointsPoincare = new GPointsArray(nPoints1);
  GPointsArray pointsRTOR = new GPointsArray(nPoints1);
  GPointsArray pointsHist = new GPointsArray(30);
  
  size(640, 360, JAVA2D);
  //fullScreen();
  frameRate(30);
   
  // ch
  heightHeader=30;
  println("Height:"+height);

  totalPlotsHeight=height-heightHeader;
  
  makeGUI();
  
  plotTach = new GPlot(this);
  plotTach.setPos(6,heightHeader);
  plotTach.setDim(width*0.55, totalPlotsHeight*0.4);
  plotTach.setMar(25, 50, 3, 0);
  
  plotTach.getYAxis().setFontSize(8);
  plotTach.getXAxis().setFontSize(8);

  plotTach.getYAxis().setAxisLabelText("RRI(ms)");
  plotTach.getYAxis().getAxisLabel().setFontSize(11);

  plotTach.setXLim(0,60);
  plotTach.setYLim(300, 1400);
 
  plotPoincare = new GPlot(this);
  plotPoincare.setPos(6,totalPlotsHeight*0.4+heightHeader+30);
  plotPoincare.setDim(width*0.55, totalPlotsHeight*0.4);
  plotPoincare.setBgColor(color(255,255,255));
  plotPoincare.setLineColor(color(255, 255, 0));
  plotPoincare.setLineWidth(3);
  plotPoincare.setPointColor(color(0,0,255));
  plotPoincare.setMar(25, 50, 0, 0);
  
  plotPoincare.getYAxis().setFontSize(8);
  plotPoincare.getXAxis().setFontSize(8);

  plotPoincare.getYAxis().getAxisLabel().setFontSize(11);
  plotPoincare.getYAxis().getAxisLabel().setFontSize(11);
  plotPoincare.getXAxis().setAxisLabelText("RRn (ms)");
  plotPoincare.getYAxis().setAxisLabelText("RRn+1 (ms)");
  plotPoincare.setXLim(300, 1300);
  plotPoincare.setYLim(300, 1300);
  
  for (int i = 0; i < 30; i++) 
  {
    pointsHist.add(i, i);
  }
  
  // Setup for the third plot 
  plotdRRHist = new GPlot(this);
  plotdRRHist.setPos(width*0.65,totalPlotsHeight*0.4+heightHeader+30);
  plotdRRHist.setDim(width*0.33, totalPlotsHeight*0.4);
  plotdRRHist.setMar(25, 5, 0, 0);
  // plotdRRHist.setXLim(5, -5);
  /*
  plotdRRHist.getTitle().setTextAlignment(LEFT);
  plotdRRHist.getTitle().setRelativePos(0);
  plotdRRHist.getYAxis().getAxisLabel().setText("Relative probability");
  plotdRRHist.getYAxis().getAxisLabel().setTextAlignment(RIGHT);
  plotdRRHist.getYAxis().getAxisLabel().setRelativePos(1);
  */
  plotdRRHist.setPoints(pointsHist);
  plotdRRHist.startHistograms(GPlot.VERTICAL);
  plotdRRHist.setYLim(0, 50);
  plotdRRHist.getHistogram().setDrawLabels(true);
  //plotdRRHist.getHistogram().setRotateLabels(true);

  for (int i = 0; i < nPoints1; i++) 
  {
    pointsPoincare.add(i,0);
    pointsRTOR.add(i,0);
  }

  plotTach.setPoints(pointsRTOR);
  plotPoincare.setPoints(pointsPoincare);

  /*******  Initializing zero for buffer ****************/

  for (int i=0; i<pSize; i++) 
  {
    time = time + 1;
    xdata[i]=time;
    ecgdata[i] = 0;
  }
  for(int i=0;i<30;i++)
  {
      histdata[i] = 0;

  }
  time = 0;
  
  delay(2000);
  if(System.getProperty("os.arch").contains("arm"))
  {
    startSerial("/dev/ttyUSB0");
  }
  else
  {
    startSerial("/dev/tty.SLAB_USBtoUART");
  }
}

public void makeGUI()
{  
   cp5 = new ControlP5(this);
   
   if(width>900)
   {
     cp5.addButton("Close")
       .setValue(0)
       .setPosition(110,height-heightHeader)
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
       .setPosition(5,height-heightHeader)
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
   }
  //List l = Arrays.asList("a", "b", "c", "d", "e", "f", "g", "h");
  /* add a ScrollableList, by default it behaves like a DropdownList */
  
  if(false)
  //if(!System.getProperty("os.arch").contains("arm"))
  {
      //List portList = port.list();
      
      cp5.addScrollableList("Select Serial port")
         .setPosition(300, 5)
         .setSize(300, 100)
         .setFont(createFont("Impact",15))
         .setBarHeight(50)
         .setItemHeight(40)
         .addItems(port.list())
         .setType(ScrollableList.DROPDOWN) // currently supported DROPDOWN and LIST
         .addCallback(new CallbackListener() 
         {
            public void controlEvent(CallbackEvent event) 
            {
              if (event.getAction() == ControlP5.ACTION_RELEASED) 
              {
                startSerial(event.getController().getLabel());
              }
            }
         } 
       );     
    }
  
    if(width<=900)
    {
       lblTCG = cp5.addTextlabel("lblTCG")
          .setText("R-R Tachogram")
          .setPosition(width*0.4,50)
          .setColorValue(color(0))
          .setFont(createFont("Impact",20));
    }
    else
    {
         lblTCG = cp5.addTextlabel("lblHR")
          .setText("R-R Tachogram")
          .setPosition(width-200,50)
          .setColorValue(color(0))
          .setFont(createFont("Impact",40));
    }
    
    if(width<=900)
    {
       lblRRI = cp5.addTextlabel("lblRRI")
          .setText("Current R-R I:")
          .setPosition(width*0.65, totalPlotsHeight*0.2)
          .setColorValue(color(255,255,255))
          .setFont(createFont("Impact",20));
    }
    else
    {
         lblRRI = cp5.addTextlabel("lblRRI")
          .setText("R-R I: ")
          .setPosition(width-250, totalPlotsHeight*0.6)
          .setColorValue(color(255,255,255))
          .setFont(createFont("Impact",40));
    }
    
    if(width<=900)
    {
      lblArrStatus = cp5.addTextlabel("label")
          .setText("Unknown")
          .setColorLabel(color(0,255,0))
          .setColorBackground(color(0,255,0))
          .setPosition(width*0.7, totalPlotsHeight*0.4)
          .setColorValue(0xffffff00)
          .setFont(createFont("Impact",30));
    }
    else
    {
      lblArrStatus = cp5.addTextlabel("label")
          .setText("Unknown")
          .setColorLabel(color(0,255,0))
          .setColorBackground(color(0,255,0))
          .setPosition(width*0.2, totalPlotsHeight*0.4)
          .setColorValue(0xffffff00)
          .setFont(createFont("Impact",100));
    }
    
     cp5.addButton("logo")
     .setPosition(5,5)
     .setImages(loadImage("protocentral.png"), loadImage("protocentral.png"), loadImage("protocentral.png"))
     .updateSize();
}

public void draw() 
{
  background(0);

  GPointsArray pointsRR = new GPointsArray(nPoints1);
  GPointsArray pointsPoincare = new GPointsArray(nPoints1);
  GPointsArray pointsHistogram = new GPointsArray(30);

  if (startPlot)                             // If the condition is true, then the plotting is done
  {
    for(int i=0; i<nPoints1-1;i++)
    {    
      pointsRR.add(i,ecgdata[i]);
      pointsPoincare.add(ecgdata[i],ecgdata[i+1]);
    }
    for(int i=0;i<30;i++)
    {
      pointsHistogram.add(i,histdata[i]);
    }
  } 
  else                                     // Default value is set
  {
  }

  plotTach.setPoints(pointsRR);
  plotPoincare.setPoints(pointsPoincare);
  plotdRRHist.setPoints(pointsHistogram);

  plotTach.beginDraw();
  plotTach.drawBackground();
  //plotTach.drawTitle();
  plotTach.drawLines();
  plotTach.drawXAxis();
  plotTach.drawYAxis();
  plotTach.drawPoints();
  plotTach.endDraw();
  
  //plotTach.defaultDraw();
  
  plotPoincare.beginDraw();
  plotPoincare.drawBackground();
  plotPoincare.drawPoints();
  plotPoincare.drawXAxis();
  plotPoincare.drawYAxis();
  //plotPoincare.drawLines();
  plotPoincare.endDraw();
  
    // Draw the third plot  
  plotdRRHist.beginDraw();
  plotdRRHist.drawBackground();
  //plotdRRHist.drawBox();
  //plotdRRHist.drawYAxis();
  plotdRRHist.drawXAxis();
  plotdRRHist.drawTitle();
  plotdRRHist.drawHistograms();
  plotdRRHist.endDraw();
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
      bufferedWriter.write("TimeStamp,ECG,SpO2,Respiration");
      bufferedWriter.newLine();
    }
  }
  catch(Exception e)
  {
    println("File Not Found");
  }
}
void startSerial(String startPortName)
{
  try
  {
      port = new Serial(this,startPortName, 115200);
      port.clear();
      startPlot = true;
  }
  catch(Exception e)
  {

    showMessageDialog(null, "Port is busy", "Alert", ERROR_MESSAGE);
    System.exit (0);
  }
}

void serialEvent (Serial blePort) 
{
  inString = blePort.readChar();
  ecsProcessData(inString);
}

void ecsProcessData(char rxch)
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
    //    println("inside 3");
    ecs_rx_state = CESState_PktLen_Found;
    CES_Pkt_Len = (int) rxch;
    CES_Pkt_Pos_Counter = CES_CMDIF_IND_LEN;
    CES_Data_Counter = 0;
    break;

  case CESState_PktLen_Found:
    //    println("inside 4");
    CES_Pkt_Pos_Counter++;
    if (CES_Pkt_Pos_Counter < CES_CMDIF_PKT_OVERHEAD)  //Read Header
    {
      if (CES_Pkt_Pos_Counter==CES_CMDIF_IND_LEN_MSB)
        CES_Pkt_Len = (int) ((rxch<<8)|CES_Pkt_Len);
      else if (CES_Pkt_Pos_Counter==CES_CMDIF_IND_PKTTYPE)
        CES_Pkt_PktType = (int) rxch;
    } else if ( (CES_Pkt_Pos_Counter >= CES_CMDIF_PKT_OVERHEAD) && (CES_Pkt_Pos_Counter < CES_CMDIF_PKT_OVERHEAD+CES_Pkt_Len+1) )  //Read Data
    {
      if (CES_Pkt_PktType == 2)
      {
        CES_Pkt_Data_Counter[CES_Data_Counter++] = (char) (rxch);          // Buffer that assigns the data separated from the packet
      }
    } else  //All  and data received
    {
      if (rxch==CES_CMDIF_PKT_STOP)
      {     
        CES_Pkt_ECG_Counter[0] = CES_Pkt_Data_Counter[0];
        CES_Pkt_ECG_Counter[1] = CES_Pkt_Data_Counter[1];
     
        int rtor = (int)CES_Pkt_Data_Counter[0] | CES_Pkt_Data_Counter[1] <<8 ;
        
        int arr_detect = (int)CES_Pkt_Data_Counter[2]; 
        println(arr_detect);

        if(arr_detect==0)
        {
          lblArrStatus.setText("Normal");
          lblArrStatus.setColor(color(0,255,0));
        }
        else
        {
          lblArrStatus.setText("Arrhythmia\nDetected");
          lblArrStatus.setColor(color(255,165,0));
        }
        time = time+1;
        xdata[arrayIndex] = time;
        
        lblRRI.setText("R-R I: "+rtor + " ms");
        ecgdata[arrayIndex] = (float)rtor;
        
        for(int i=0;i<30;i++)
        {
            histdata[i]=CES_Pkt_Data_Counter[3+i];  
        }

        arrayIndex++;
       
        
        if (arrayIndex == pSize)
        {  
          arrayIndex = 0;
          time = 0;
        }       

        // If record button is clicked, then logging is done

        if (logging == true)
        {
          try 
          {
            date = new Date();
            dateFormat = new SimpleDateFormat("HH:mm:ss");
            bufferedWriter.write(dateFormat.format(date)+","+ecg+","+spo2+","+resp);
            bufferedWriter.newLine();
          }
          catch(IOException e) 
          {
            println("It broke!!!");
            e.printStackTrace();
          }
        }
        ecs_rx_state=CESState_Init;
      } 
      else
      {
        ecs_rx_state=CESState_Init;
      }
    }
    break;

  default:
    break;
  }
}

/*********************************************** Recursive Function To Reverse The data *********************************************************/

public int reversePacket(char DataRcvPacket[], int n)
{
  if (n == 0)
    return (int) DataRcvPacket[n]<<(n*8);
  else
    return (DataRcvPacket[n]<<(n*8))| reversePacket(DataRcvPacket, n-1);
}

/*************** Function to Calculate Average *********************/
double averageValue(float dataArray[])
{

  float total = 0;
  for (int i=0; i<dataArray.length; i++)
  {
    total = total + dataArray[i];
  }
  return total/dataArray.length;
}