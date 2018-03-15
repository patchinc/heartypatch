# coding: utf-8

#
#  HeartyPatch Client
#
# Copyright Douglas Williams, 2018
#
# Licensed under terms of MIT License (http://opensource.org/licenses/MIT).
#

# To Do:
# 1) Support incremental Saves


import socket
from pprint import pprint
import os
import sys
import signal as sys_signal
import struct

import numpy as np
import matplotlib.pyplot as plt
import scipy.signal as signal
import time
from datetime import datetime

hp_host = '192.168.0.106'
hp_port = 4567
fname = 'log.csv'


# from: https://stackoverflow.com/questions/20766813/how-to-convert-signed-to-unsigned-integer-in-python
def is_interactive():
    import __main__ as main
    return not hasattr(main, '__file__')


class HeartyPatch_TCP_Parser:
    # Packet Validation
    CESState_Init = 0
    CESState_SOF1_Found = 1
    CESState_SOF2_Found = 2
    CESState_PktLen_Found = 3

    # CES CMD IF Packet Format
    CES_CMDIF_PKT_START_1 = 0x0A
    CES_CMDIF_PKT_START_2 = 0xFA
    CES_CMDIF_PKT_STOP = 0x0B

    # CES CMD IF Packet Indices
    CES_CMDIF_IND_LEN = 2
    CES_CMDIF_IND_LEN_MSB = 3
    CES_CMDIF_IND_PKTTYPE = 4
    CES_CMDIF_PKT_OVERHEAD = 5
    CES_CMDIF_PKT_DATA = CES_CMDIF_PKT_OVERHEAD


    ces_pkt_seq_bytes   = 4  # Buffer for Sequence ID
    ces_pkt_ts_bytes   = 8  # Buffer for Timestamp
    ces_pkt_rtor_bytes = 4  # R-R Interval Buffer
    ces_pkt_ecg_bytes  = 4  # Field(s) to hold ECG data

    Expected_Type = 3        # new format
    
    min_packet_size = 19
    
    def __init__(self):
        self.state = self.CESState_Init
        self.data = ''
        self.packet_count = 0
        self.bad_packet_count = 0
        self.bytes_skipped = 0
        self.total_bytes = 0
        self.all_seq = []
        self.all_ts = []
        self.all_rtor = []
        self.all_hr = []
        self.all_ecg = []
        pass
    
    def add_data(self, new_data):
        self.data += new_data
        self.total_bytes += len(new_data)
    
    
    def process_packets(self):
        while len(self.data) >= self.min_packet_size:
            if self.state == self.CESState_Init:
                if ord(self.data[0]) == self.CES_CMDIF_PKT_START_1:
                    self.state = self.CESState_SOF1_Found
                else:
                    self.data = self.data[1:]    # skip to next byte
                    self.bytes_skipped += 1
                    continue
            elif self.state == self.CESState_SOF1_Found:
                if ord(self.data[1]) == self.CES_CMDIF_PKT_START_2:
                    self.state = self.CESState_SOF2_Found
                else:
                    self.state = self.CESState_Init
                    self.data = self.data[1:]    # start from beginning
                    self.bytes_skipped += 1
                    continue
            elif self.state == self.CESState_SOF2_Found:
                # sanity check header for expected values
                
                pkt_len = 256 * ord(self.data[self.CES_CMDIF_IND_LEN_MSB]) + ord(self.data[self.CES_CMDIF_IND_LEN])
                # Make sure we have a full packet
                if len(self.data) < (self.CES_CMDIF_PKT_OVERHEAD + pkt_len + 2):
                    break


                if (ord(self.data[self.CES_CMDIF_IND_PKTTYPE])  != self.Expected_Type
                    or ord(self.data[self.CES_CMDIF_PKT_OVERHEAD+pkt_len+1]) != self.CES_CMDIF_PKT_STOP):

                    if True:
                        print 'pkt_len', pkt_len
                        print ord(self.data[self.CES_CMDIF_IND_PKTTYPE]), self.Expected_Type,
                        print ord(self.data[self.CES_CMDIF_IND_PKTTYPE])  != self.Expected_Type

                        for j in range(0, self.CES_CMDIF_PKT_OVERHEAD):
                            print format(ord(self.data[j]),'02x'),
                        print

                        for j in range(self.CES_CMDIF_PKT_OVERHEAD, self.CES_CMDIF_PKT_OVERHEAD+pkt_len):
                            print format(ord(self.data[j]),'02x'),
                        print

                        for j in range(self.CES_CMDIF_PKT_OVERHEAD+pkt_len, self.CES_CMDIF_PKT_OVERHEAD+pkt_len+2):
                            print format(ord(self.data[j]),'02x'),
                        print
                        print self.CES_CMDIF_PKT_STOP,
                        print ord(self.data[self.CES_CMDIF_PKT_OVERHEAD+pkt_len+2]) != self.CES_CMDIF_PKT_STOP
                        print

                    # unexpected packet format
                    self.state = self.CESState_Init
                    self.data = self.data[1:]    # start from beginning
                    self.bytes_skipped += 1
                    self.bad_packet_count += 1
                    continue

                # Parse Payload
                payload = self.data[self.CES_CMDIF_PKT_OVERHEAD:self.CES_CMDIF_PKT_OVERHEAD+pkt_len+1]

                ptr = 0
                # Process Sequence ID
                seq_id = struct.unpack('<I', payload[ptr:ptr+4])[0]
                self.all_seq.append(seq_id)
                ptr += self.ces_pkt_seq_bytes

                # Process Timestamp
                ts_s = struct.unpack('<I', payload[ptr:ptr+4])[0]
                ts_us = struct.unpack('<I', payload[ptr+4:ptr+8])[0]
                timestamp = ts_s + ts_us/1000000.0
                self.all_ts.append(timestamp)
                ptr += self.ces_pkt_ts_bytes

                # Process R-R Interval
                rtor = struct.unpack('<I', payload[ptr:ptr+4])[0]
                self.all_rtor.append(rtor)
                if rtor == 0:
                    self.all_hr.append(0)
                else:
                    self.all_hr.append(60000.0/rtor)

                ptr += self.ces_pkt_rtor_bytes


                assert ptr == 16
                assert pkt_len == (16 + 8 * 4)
                # Process Sequence ID
                while ptr < pkt_len:
                    ecg = struct.unpack('<i', payload[ptr:ptr+4])[0] / 1000.0
                    self.all_ecg.append(ecg)
                    ptr += self.ces_pkt_ecg_bytes

                self.packet_count += 1                    
                self.state = self.CESState_Init
                self.data = self.data[self.CES_CMDIF_PKT_OVERHEAD+pkt_len+2:]    # start from beginning


soc = None
hp = None
tStart = None
def get_heartypatch_data(max_packets=10000, hp_host='192.168.0.106', max_seconds=-1):
    global soc
    global hp
    global tStart

    tcp_reads = 0

    hp = HeartyPatch_TCP_Parser()
    print hp_host

    try:
        soc = socket.create_connection((hp_host, hp_port))
    except Exception:
        try:
            soc.close()
        except Exception:
            pass
        soc = socket.create_connection((hp_host, hp_port))

    i = 0
    pkt_last = 0
    txt = soc.recv(16*1024)  # discard any initial results
    tStart = time.time()
    while max_packets == -1 or hp.packet_count < max_packets:
        txt = soc.recv(16*1024)
        hp.add_data(txt)
        hp.process_packets()
        i += 1

        tcp_reads += 1
        if tcp_reads % 50 == 0:
            print '.',
            sys.stdout.flush()

        if hp.packet_count - pkt_last >=  1000:
            pkt_last = pkt_last + 1000
            print hp.packet_count//1000,
            sys.stdout.flush()
        if time.time() - tStart > max_seconds:
            break


def finish(show_plot):
    global soc
    global hp
    global tStart
    global fname

    duration = time.time() - tStart
    if soc is not None:
        soc.close()
    
    packet_rate = float(hp.packet_count)/duration
    ecg_sample_rate = float(len(hp.all_ecg)) / duration
    print
    print 'Recording duration: {:0.1f} sec  -- Packet Rate: {:0.1f} pkt/sec  ECG Sample Rate: {:0.1f} samp/sec'.format(
        duration, packet_rate, ecg_sample_rate)
    print 'Packets: {}  Bytes_per_packet: {}  Bad Packets: {}  Bytes Skipped: {}'.format(
        hp.packet_count, hp.total_bytes/float(hp.packet_count), hp.bad_packet_count, hp.bytes_skipped)
    print
    if len(hp.all_ecg) > 0:
        print 'Signal -- Max: {:0.0f}  Min: {:0.0f}  Range: {:0.0f}'.format(
            np.max(hp.all_ecg), np.min(hp.all_ecg), np.max(hp.all_ecg)- np.min(hp.all_ecg))
        print 'Index of largest and smallest values -- max: {} min:{}'.format(
            np.argmax(hp.all_ecg), np.argmin(hp.all_ecg))


    ptr = fname.rfind('.')
    fname_ecg =  fname[:ptr] + '_ecg' + fname[ptr:]

    header = '{} Epoch: {}  Duration: {:0.1f} sec  Rate: {:0.1f}'.format(
        time.ctime(tStart), tStart, duration, ecg_sample_rate)
    np.savetxt(fname_ecg, hp.all_ecg, header=header)

    header = 'seq, timestamp, rtor, hr'
    np.savetxt(fname, zip(hp.all_seq, hp.all_ts, hp.all_rtor, hp.all_hr),
               fmt=('%d','%.3f','%d','%d'), header=header, delimiter=',')

    for i in range(1, len(hp.all_seq)):
        if hp.all_seq[i] != hp.all_seq[i-1] + 1:
            print 'gap at:', i, hp.all_ts[i-1], hp.all_ts[i]

    print 'Timestamps -- duration: {}  start: {}  end: {}'.format(
        hp.all_ts[-1]- hp.all_ts[0], hp.all_ts[0], hp.all_ts[-1])

    if show_plot:
        n = int(5*ecg_sample_rate)
        plt.figure(figsize=(11, 2))
        plt.title('HeartyPatch Output -- First 5 seconds')
        plt.plot(np.arange(n)*5.0/n,   hp.all_ecg[:n])
        plt.ylabel('ecg')
        plt.xlabel('time in sec')
        plt.xlim(0, )
        plt.show()

        n = int(5*60 * packet_rate)
        subset_hr = hp.all_hr[:n]

        plt.figure(figsize=(11, 2))
        plt.title('HeartyPatch Output -- First 5 minute')
        plt.plot(np.arange(len(subset_hr))*5.0/n, subset_hr)
        plt.ylabel('HR')
        plt.xlabel('time in min')
        plt.xlim(0, )
        plt.ylim(0, 240)
        plt.show()




def signal_handler(signal, frame):
    global soc
    print('Interrupted by Ctrl+C!')
    finish()
    sys.exit(0)


def help():
    print 'usage: python heartypatch_downloader_protocol3.py -f <fname> -s <total samples> -m <total_minutes> -i <ip address> -n'
    print 'Where:'
    print '    -f <fname> is filename, used tocopatch_recordings folder and .csv suffix by default'
    print '    -s <total samples> -- total sample count, not needed is minutes specified'
    print '    -m <total_minutes> -- recording duration im minutes (defaault 10min)'
    print '    -i <ip address>  full IP address or address within in 192.168.43.x subnet>'
    print '    -p show plot upon completion of recording'
    sys.exit(0)

if __name__ == "__main__" and not is_interactive():
    max_packets= -1
    max_seconds = 10*60  # default recording duration is 10min
    fname = 'log.csv'
    hp_host = 'heartypatch.local'
    show_plot = False
    
    i = 1
    while i < len(sys.argv):
        if sys.argv[i] == '-f' and i < len(sys.argv)-1:
            fname = sys.argv[i+1]
            i += 2
        elif sys.argv[i] == '-s' and i < len(sys.argv)-1:
            max_packets = int(sys.argv[i+1])
            max_seconds = -1
            i += 2
        elif sys.argv[i] == '-m' and i < len(sys.argv)-1:
            max_seconds = int(sys.argv[i+1])*60
            max_packets = -1
            i += 2
        elif sys.argv[i] == '-i' and i < len(sys.argv)-1:
            try:
                foo = int(sys.argv[i+1])
                hp_host = '192.168.43.'+sys.argv[i+1]
            except Exception:
                hp_host = sys.argv[i + 1]
            i += 2
        elif sys.argv[i] in '-p':
            show_plot = True
            i += 1
        elif sys.argv[i] in ['-h', '--help']:
            help()
        else:
            print 'Unknown argument', sys.argv[i]
            help()

    sys_signal.signal(sys_signal.SIGINT, signal_handler)
    get_heartypatch_data(max_packets=max_packets, max_seconds=max_seconds, hp_host=hp_host)
    finish(show_plot)


