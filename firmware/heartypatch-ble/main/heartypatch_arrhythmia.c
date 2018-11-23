#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "bt.h"
#include "driver/ledc.h"
#include "driver/uart.h"

#include "heartypatch_max30003.h"
#include "heartypatch_ble.h"
#include "heartypatch_arrhythmia.h"

uint8_t arrhythmiadetector = 0x00;

float rr_new[MAX-1][2]={0};
float sum[MAX-1]={0};
float os[MAX-1]={0};
float drr[MAX-1][2]={0};
float drr_s[MAX-1]={0};
float drrnew[MAX-1][2]={0};
unsigned int z[30][30]={0};
unsigned int z2[30][30]={0};
unsigned int z3[30][30]={0};
unsigned int flipz3[30][30]={0};
unsigned int flipz4[30][30]={0};
unsigned int z4[30][30]={0};
unsigned int z1[30][30]={0};

int origincount=0;
int i,j,h,y=0;

int		BC1		= 		0;
int		BC2		= 		0;
int		BC3		= 		0;
int		BC4		= 		0;
int		BC5		= 		0;
int		BC6		= 		0;
int		BC7		= 		0;
int		BC8		= 		0;
int		BC9		= 		0;
int		BC10	= 		0;
int		BC11	= 		0;
int		BC12	= 		0;
int		PC1		= 		0;
int		PC2		= 		0;
int		PC3		= 		0;
int		PC4		= 		0;
int		PC5		= 		0;
int		PC6		= 		0;
int		PC7		= 		0;
int		PC8		= 		0;
//int		PC9		= 		0;
int		PC10	= 		0;
//int		PC11	= 		0;
int		PC12	= 		0;

int a=0;
float start = -0.58;
int IrrEv;
int PACEv;
float AFEv;
int BC=0;
int PC=0;
int bdc=0;
int pdc=0;
int n=14;
int tally =0;
float verify=0;
//float comput_AFEv(float array_temp[]); //conflicting types error

void challenge(float array_temp[])
{
	AFEv=0.00;
	AFEv = comput_AFEv(array_temp);
//	printf("%f\n",AFEv);
	if(AFEv>1)
	{
		arrhythmiadetector= 0xff;
	}
	else
	{
		arrhythmiadetector = 0x1f;
	}
	
}
float comput_AFEv(float array_temp[])
{
	drrf(array_temp);
	metrics(drr_s);
	AFEv=IrrEv-origincount-2*PACEv;
	return AFEv;
}
void drrf(float array_temp[])
{
	for(i=0;i<(MAX-1);i++)
	{
		rr_new[i][0] = array_temp[i+1];
		rr_new[i][1] = array_temp[i];
        if((rr_new[i][0]<0.5)||(rr_new[i][1]<0.5))
		{
			drr_s[i] = 2*(rr_new[i][0]-rr_new[i][1]);
		}
		else if((rr_new[i][0]>1)||(rr_new[i][1]>1))
		{
			drr_s[i] = 0.5*(rr_new[i][0]-rr_new[i][1]);

		}
		else
		{	
			drr_s[i]= (rr_new[i][0]-rr_new[i][1]);
	    }
	}

}
void metrics( float drr_s[])
{
	origincount = 0;
	tally=0;
	IrrEv=0;
	PACEv=0;

	int		BC1		= 		0;
	int		BC2		= 		0;
	int		BC3		= 		0;
	int		BC4		= 		0;
	int		BC5		= 		0;
	int		BC6		= 		0;
	int		BC7		= 		0;
	int		BC8		= 		0;
	int		BC9		= 		0;
	int		BC10	= 		0;
	int		BC11	= 		0;
	int		BC12	= 		0;
	int		PC1		= 		0;
	int		PC2		= 		0;
	int		PC3		= 		0;
	int		PC4		= 		0;
	int		PC5		= 		0;
	int		PC6		= 		0;
	int		PC7		= 		0;
	int		PC8		= 		0;
//	int		PC9		= 		0;
	int		PC10	= 		0;
//	int		PC11	= 		0;
	int		PC12	= 		0;

	for(i=0;i<(MAX-2);i++)
	{
		drr[i][0] = drr_s[i+1];
		drr[i][1] = drr_s[i];
		if(((fabs(drr[i][0]))<=0.02)&&((fabs(drr[i][1]))<=0.02))
	    {
			origincount++;
        }
	}

  	for(i=0;i<(MAX-2);i++)
	{
		if((fabs(drr[i][0])<=1.5)&&(fabs(drr[i][1])<=1.5))
	    {
		drrnew[tally][0]=drr[i][0];
	    drrnew[tally][1]=drr[i][1];
		tally++;
	    }
	}
	for(i=0;i<30;i++)
	{
		for(j=0;j<30;j++)
		{
			z[i][j]=0;
		}
	}


	for(j=0;j<30;j++)
	{
		for(h=0;h<30;h++)
		{
			for(i=0;i<tally;i++)
			{
				if(((drrnew[i][0])>(-0.60+0.04*(float) j))&&((drrnew[i][0])<=(-0.60+0.04*(((float) j)+1)))&&((drrnew[i][1])>(-0.60+0.04*(float) h))&&((drrnew[i][1])<=(-0.60+0.04*(((float) h)+1))))
				{
					z[j][h]++;
				}
			}
		}
	}

	for(h=0;h<30;h++)
	{
		for(i=0;i<tally;i++)
		{
			if((drrnew[i][0]<=(-0.60))&&(drrnew[i][1]<=(-0.60)))
			{
					z[0][0]++;

			}

			else if((drrnew[i][0]<=(-0.60))&&((drrnew[i][1])>(-0.60+0.04*h))&&((drrnew[i][1])<=(-0.60+0.04*(h+1))))
			{
					z[0][h]++;

			}
		}
	}


	for(h=0;h<30;h++)
	{
		for(i=0;i<tally;i++)
		{
		    if((drrnew[i][0]>(0.60))&&(drrnew[i][1]>(0.60)))

				{
					z[29][29]++;

				}

			else if((drrnew[i][0]>(0.60))&&((drrnew[i][1])>(-0.60+0.04*h))&&((drrnew[i][1])<=(-0.60+0.04*(h+1))))
				{

					z[29][h]++;


				}
		}

	}


	for(h=0;h<30;h++)
	{
		for(i=0;i<tally;i++)
		{
		   if((drrnew[i][1]>(0.60))&&((drrnew[i][0])>(-0.60+0.04*h))&&((drrnew[i][0])<=(-0.60+0.04*(h+1))))
			{
				z[h][29]++;

			}
		}
	}

	for(h=0;h<30;h++)
	{
			for(i=0;i<tally;i++)
			{
			   if((drrnew[i][1]<=(-0.60))&&((drrnew[i][0])>(-0.60+0.04*h))&&((drrnew[i][0])<=(-0.60+0.04*(h+1))))
				{

					z[h][0]++;


				}
			}

	}
	for(i=0;i<tally;i++)
	{
			    if((drrnew[i][0]>(0.60))&&(drrnew[i][1]<=(-0.60)))
				{
					z[29][0]++;
				}
			    if((drrnew[i][0]<=(-0.60))&&(drrnew[i][1]>(0.60)))
				{
					z[0][29]++;
				}
	}

	//clearing segments to zero,god only knows why..:/
	z[13][14]=0;
	z[13][15]=0;
	z[14][13]=0;
	z[14][14]=0;
	z[14][15]=0;
	z[14][16]=0;
	z[15][13]=0;
	z[15][14]=0;
	z[15][15]=0;
	z[15][16]=0;
	z[16][14]=0;
	z[16][15]=0;

	//fourth quadrant of z
	h=0;
	a=0;

	for(i=15;i<30;i++)
	{
		for(j=15;j<30;j++)
		{
			z2[a][h]=z[i][j];
			h++;
		}
		a++;
		h=0;
	}

	bpcount(z2);
	// taking the values from bpcount
	BC12 = BC;
	PC12 = PC;

	//replacing back z with the values of z2
	a=0;
	h=0;

	for(i=15;i<30;i++)
	{
		for(j=15;j<30;j++)
		{
			z[i][j]=z2[a][h];
			h++;
		}
		a++;
		h=0;
	}

	//third quadrant  of z
	h=0;
	a=0;
	for(i=15;i<30;i++)
	{
		for(j=0;j<15;j++)
		{
			z2[a][h]=z[i][j];
			h++;
		}
		a++;
		h=0;
	}

	for(i=0;i<15;i++)
	{ a=14;
	  for(j=0;j<15;j++)
	  {
		  flipz3[i][j] = z2[i][a];
		  a--;
	  }
	}

	bpcount(flipz3);
	// taking the values from bpcount
	BC11 = BC;
	//PC11 = PC;

	for(i=0;i<15;i++)
	{ a=14;
	  for(j=0;j<15;j++)
	  {
		  z2[i][j] = flipz3[i][a];
		  a--;
	  }
	}

	//replacing back z with the values of z2
	a=0;
	h=0;

	for(i=15;i<30;i++)
	{
		for(j=0;j<15;j++)
		{
			z[i][j]=z2[a][h];
			h++;
		}
		a++;
		h=0;
	}

	//first quadrant of z
	h=0;
	a=0;

	for(i=0;i<15;i++)
	{
		for(j=0;j<15;j++)
		{
			z2[a][h]=z[i][j];
			h++;
		}
		a++;
		h=0;
	}

	bpcount(z2);
	// taking the values from bpcount
	BC10 = BC;
	PC10 = PC;

	//replacing back z with the values of z2
	a=0;
	h=0;

	for(i=0;i<15;i++)
	{
		for(j=0;j<15;j++)
		{
			z[i][j]=z2[a][h];
			h++;
		}
		a++;
		h=0;
	}
	// second quadrant of z

	h=0;
	a=0;
	for(i=0;i<15;i++)
	{
		for(j=15;j<30;j++)
		{
			z2[a][h]=z[i][j];
			h++;
		}
		a++;
		h=0;
	}

	for(i=0;i<15;i++)
	{ a=14;
	  for(j=0;j<15;j++)
	  {
		  flipz4[i][j] = z2[i][a];
		  a--;
	  }
	}

	bpcount(flipz4);
	BC9 = BC;
	//PC9 = PC;

	// taking the values from bpcount
	for(i=0;i<15;i++)
	{ a=14;
	  for(j=0;j<15;j++)
	  {
		  z2[i][j] = flipz4[i][a];
		  a--;
	  }
	}

	//replacing back z with the values of z2
	a=0;
	h=0;

	for(i=0;i<15;i++)
	{
		for(j=15;j<30;j++)
		{
			z[i][j]=z2[a][h];
			h++;
		}
		a++;
		h=0;
	}
	// counting bins

	//bin count 5
	for(i=0;i<15;i++)
	{
		for(j=13;j<17;j++)
		{
			if(z[i][j]!= 0)
			{BC5++;}

			PC5 = PC5+z[i][j];

		}
	}


	for(i=15;i<30;i++)
	{
		for(j=13;j<17;j++)
		{
			if(z[i][j]!= 0)
			{BC7++;}
			PC7 = PC7+z[i][j];
		}
	}

	for(i=13;i<17;i++)
	{
		for(j=0;j<15;j++)
		{
			if(z[i][j]!= 0)
			{BC6++;}
			PC6 = PC6+z[i][j];
		}
	}

	for(i=13;i<17;i++)
	{
		for(j=15;j<30;j++)
		{
			if(z[i][j]!= 0)
			{BC8++;}
			PC8 = PC8+z[i][j];
		}
	}

	// clearing segments 5,6,7,8
	for(j=0;j<30;j++)
	{
		for(i=13;i<17;i++)
		{
			z[i][j]=0;
		}
	}

	for(i=0;i<30;i++)
	{
		for(j=13;j<17;j++)
		{
			z[i][j]=0;
		}
	}

	PC2=0;
	for(i=0;i<13;i++)
	{
		for(j=0;j<13;j++)
		{
			if(z[i][j]!= 0)
			{
				BC2++;
			}
			PC2 = PC2+z[i][j];
		}
	}

	PC1=0;
	for(i=0;i<13;i++)
	{
		for(j=17;j<30;j++)
		{
			if(z[i][j]!= 0)
			{
				BC1++;
			}
			PC1 = PC1+z[i][j];
		}
	}

	PC3=0;
	for(i=17;i<30;i++)
	{
		for(j=0;j<13;j++)
		{
			if(z[i][j]!= 0)
			{
				BC3++;
			}
			PC3 = PC3+z[i][j];
		}
	}

	PC4=0;
	for(i=17;i<30;i++)
	{
		for(j=17;j<30;j++)
		{
			if(z[i][j]!= 0)
			{
				BC4++;
			}
			PC4 = PC4+z[i][j];
		}
	}

	IrrEv=BC1+BC2+BC3+BC4+BC5+BC6+BC7+BC8+BC9+BC10+BC11+BC12;
	PACEv=(PC1-BC1)+(PC2-BC2)+(PC3-BC3)+(PC4-BC4)+(PC5-BC5)+(PC6-BC6)+(PC10-BC10)-(PC7-BC7)-(PC8-BC8)-(PC12-BC12);
	y=0;
}
void bpcount(unsigned int z2[][30])
{
    unsigned int diag[15]={0};
	// otaining negative indices diagonal elements
	bdc=0;
	BC=0;
	pdc=0;
	PC=0;
	for(a=-2;a<=-1;a++)
	{
        i=fabs(a);
		for(j=0;(j<=(n-fabs(a)));j++)
		{
			diag[j]=z2[i][j];
			z2[i][j]=z2[i][j]-diag[j];
		    if (diag[j] != 0)
			{
				bdc++;
				pdc=pdc+diag[j];
			}
			i++;
		}
	}

	BC = BC+bdc;
	PC = PC+pdc;
	bdc=0;
	pdc=0;

  	// otaining postive indices diagonal elements
	for(a=1;a<=2;a++)
	{   j=a;
		for(i=0;i<=(n-a);i++)
		{
				diag[i]=z2[i][j];
				z2[i][j]=z2[i][j]-diag[i];
				if (diag[i] != 0)
				{
					bdc++;
					pdc=pdc+diag[i];
				}
				j++;

		}
	}

	BC = BC+bdc;
	PC = PC+pdc;
	bdc=0;
	pdc=0;
	i=0;

 	 // otaining main diagonal elements
	for(j=0;j<=n;j++)
	{
		diag[j]=z2[i][j];
		z2[i][j]=z2[i][j]-diag[j];
		  if (diag[j] != 0)
		{
			bdc++;
			pdc=pdc+diag[j];
		}
		i++;
	}

	BC = BC+bdc;
	PC = PC+pdc;

}
