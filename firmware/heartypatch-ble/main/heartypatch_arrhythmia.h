#ifndef heartypatch_arrhythmia_H_
#define heartypatch_arrhythmia_H_

void challenge(float array_temp[]);
float comput_AFEv(float array_temp[]);
void drrf(float array_temp[]);
void metrics( float drr_s[]);
void bpcount(unsigned int z2[][30]);

#endif