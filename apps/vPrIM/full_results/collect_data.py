import numpy as np
import sys
import os
import re
import pandas as pd
import math
import json
import csv

overhead_avg_1 = 0
overhead_avg_60 = 0

def append_to_file(filename, string):
    file = open(filename,"a+")
    file.write(string)
    file.close()

def get_template():
    template_file = open("tex_template/pgf_main", "r")
    template = template_file.read()
    template_file.close()
    return template

def get_template_no_overhead():
    template_file = open("tex_template/pgf_main_2", "r")
    template = template_file.read()
    template_file.close()
    return template


def add_legends(filename):
    legends_file = open("tex_template/pgf_head", "r")
    legends = legends_file.read()
    append_to_file(filename, legends+"\n\n")
    legends_file.close()

def add_end(filename):
    end_file = open("tex_template/pgf_foot", "r")
    end = end_file.read()
    append_to_file(filename, end+"\n\n")
    end_file.close()

def read_entries(path):
    entries = {'CPU-DPU': 0, 'DPU': 0, 'Inter-DPU': 0, 'DPU-CPU': 0}
    try:
        print(path)
        file = open(path, "r")
        line = file.readline()
        while line[0]!="\t":
            line = file.readline()
        while "\t" in line and ('DPU' in line or 'CPU' in line):
            parts = line.split(" ")
            entries[parts[0][1:-1]] = float(parts[1][0:-2])
            line = file.readline()
        file.close()
        return entries
    except IOError:
        return entries
    
def get_data(apps, vpim_path):
    hashmap = {'native': {'60': {}, '480': {}}, 'VPIM': {'60': {}, '480': {}}}
    for app in apps:
        hashmap['native']['60'][app] = read_entries('native/'+app+"_60")
        hashmap['native']['480'][app] = read_entries('native/'+app+"_480")
        hashmap['VPIM']['60'][app] = read_entries(vpim_path+app+"_60")
        hashmap['VPIM']['480'][app] = read_entries(vpim_path+app+"_480")
    return hashmap

def create_pfg(type, hashmap, i, with_overhead):
    native_data = hashmap["native"]
    vpim_data = hashmap["VPIM"]
    template = ""
    if with_overhead:
        template = get_template()
    else:
        template = get_template_no_overhead()

    template = template.replace("#CPU-DPU-60-native", str(native_data["60"][type]["CPU-DPU"]))
    template = template.replace("#DPUKernel-60-native", str(native_data["60"][type]["DPU"]))
    template = template.replace("#InterDPU-60-native", str(native_data["60"][type]["Inter-DPU"]))
    template = template.replace("#DPU-CPU-60-native", str(native_data["60"][type]["DPU-CPU"]))
    
    template = template.replace("#CPU-DPU-60-vpim", str(vpim_data["60"][type]["CPU-DPU"]))
    template = template.replace("#DPUKernel-60-vpim", str(vpim_data["60"][type]["DPU"]))
    template = template.replace("#InterDPU-60-vpim", str(vpim_data["60"][type]["Inter-DPU"]))
    template = template.replace("#DPU-CPU-60-vpim", str(vpim_data["60"][type]["DPU-CPU"]))
    
    template = template.replace("#CPU-DPU-480-native", str(native_data["480"][type]["CPU-DPU"]))
    template = template.replace("#DPUKernel-480-native", str(native_data["480"][type]["DPU"]))
    template = template.replace("#InterDPU-480-native", str(native_data["480"][type]["Inter-DPU"]))
    template = template.replace("#DPU-CPU-480-native", str(native_data["480"][type]["DPU-CPU"]))
    
    template = template.replace("#CPU-DPU-480-vpim", str(vpim_data["480"][type]["CPU-DPU"]))
    template = template.replace("#DPUKernel-480-vpim", str(vpim_data["480"][type]["DPU"]))
    template = template.replace("#InterDPU-480-vpim", str(vpim_data["480"][type]["Inter-DPU"]))
    template = template.replace("#DPU-CPU-480-vpim", str(vpim_data["480"][type]["DPU-CPU"]))

    sums = []
    for config in native_data:
        sum = 0
        for step in native_data[config][type]:
            sum+=native_data[config][type][step]
        sums.append(sum)
    for config in vpim_data:
        sum = 0
        for step in vpim_data[config][type]:
            sum+=vpim_data[config][type][step]
        sums.append(sum)
    maximum = math.ceil(max(sums))
    y1 = math.ceil((maximum*0.3)/10)*10
    y2 = 2*y1
    y3 = 3*y1
    ymax = 4*y1

    template = template.replace("#ymax", str(ymax))

    template = template.replace("#y1", str(y1))
    template = template.replace("#y2", str(y2))
    template = template.replace("#y3", str(y3))

    template = template.replace("#type", type)
    
    if with_overhead:
        overhead_1 = 0
        if sums[0]!=0:
            overhead_1 = sums[2]/sums[0]
        overhead_60 = 0
        if sums[1]!=0:
            overhead_60 = sums[3]/sums[1]
        global overhead_avg_1, overhead_avg_60
        overhead_avg_1 += overhead_1
        overhead_avg_60 += overhead_60
        template = template.replace("#overhead_1", str(round(overhead_1, 2)))
        template = template.replace("#overhead_60", str(round(overhead_60, 2)))
        if abs(overhead_60-overhead_1) < 0.1:
            template = template.replace("#overhead_max", str(max(overhead_60, overhead_1)*1.08))
            template = template.replace("#overhead_min", str(min(overhead_60, overhead_1)*0.92))
        else:
            template = template.replace("#overhead_max", str(max(overhead_60, overhead_1)*1.2))
            template = template.replace("#overhead_min", str(min(overhead_60, overhead_1)*0.8))

    if i%4==0:
        template = template.replace("%_left", "")
    if i%4==3:
        template = template.replace("%_right", "")
    if i>11:
        template = template.replace("%_down", "")
    
    append_to_file("pgf_output", template+"\n%---\n")

    return


def get_CT_DT(hashmap, apps):
    native_480 = hashmap["native"]["480"]
    vpim_480 = hashmap["VPIM"]["480"]
    for app in apps:
        native_CT = native_480[app]["DPU"] + native_480[app]["Inter-DPU"]
        native = native_480[app]["CPU-DPU"] + native_480[app]["DPU-CPU"] + native_CT
        vpim_CT = vpim_480[app]["DPU"] + vpim_480[app]["Inter-DPU"]
        vpim = vpim_480[app]["CPU-DPU"] + vpim_480[app]["DPU-CPU"] + vpim_CT
        print(app+": \n\tNative Total: "+str(native) + "\n\tnative CT: "+str(native_CT)+"\n\tvPIM Total: "+str(vpim) + "\n\tvPIM CT: "+str(vpim_CT))
    return


if __name__ == "__main__":
    if os.path.exists("pgf_output"):
        os.remove("pgf_output")
    apps = ['BS', 'TS', 'MLP', 'VA', 'HST-L', 'HST-S', 'GEMV', 'SCAN-RSS', 'SCAN-SSA', 'RED', 'TRNS', 'NW','SEL', 'UNI', 'SpMV', 'BFS']
    hashmap = get_data(apps, 'vPIM_1/')
    get_CT_DT(hashmap, apps)
    add_legends("pgf_output")
    for i in range(len(apps)):
        create_pfg(apps[i], hashmap, i, False)
        if (i+1)%4 == 0 and i!=len(apps)-1:
            append_to_file("pgf_output", "\n%---\n")    
    add_end("pgf_output")
    print("Overhead avg 60: "+str(overhead_avg_1/len(apps)))
    print("Overhead avg 480: "+str(overhead_avg_60/len(apps)))


    
    
