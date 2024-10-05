import numpy as np
import sys
import os
import re
import pandas as pd
import math
import json
import csv

def append_to_file(filename, string):
    file = open(filename,"a+")
    file.write(string)
    file.close()

def get_template():
    template_file = open("tex_template/pgf_main", "r")
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
    hashmap = {'native': {}, 'VPIM': {}}
    json_file = open('native.json')
    data = json.load(json_file)
    for app in apps:
        hashmap['VPIM'][app] = {}
        hashmap['VPIM'][app]['1'] = read_entries(vpim_path+app+"_1")
        hashmap['VPIM'][app]['60'] = read_entries(vpim_path+app)
        hashmap['native'][app] = {}
        hashmap['native'][app]['1'] = read_entries("native/"+app+"_1")
        hashmap['native'][app]['60'] = read_entries("native/"+app+"_60")
    
    for app in apps:
        for config in hashmap['native'][app]:
            for step in hashmap['native'][app][config]:
                print(str(app) + " "+ str(step))
                hashmap['native'][app][config][step] = max(hashmap['native'][app][config][step], data[app][config][step])
    print(hashmap)
    return hashmap

def create_pfg(type, hashmap, i):
    native_data = hashmap["native"][type]
    vpim_data = hashmap["VPIM"][type]
    template = get_template()

    template = template.replace("#CPU-DPU-1-native", str(native_data["1"]["CPU-DPU"]))
    template = template.replace("#DPUKernel-1-native", str(native_data["1"]["DPU"]))
    template = template.replace("#InterDPU-1-native", str(native_data["1"]["Inter-DPU"]))
    template = template.replace("#DPU-CPU-1-native", str(native_data["1"]["DPU-CPU"]))
    
    template = template.replace("#CPU-DPU-1-vpim", str(vpim_data["1"]["CPU-DPU"]))
    template = template.replace("#DPUKernel-1-vpim", str(vpim_data["1"]["DPU"]))
    template = template.replace("#InterDPU-1-vpim", str(vpim_data["1"]["Inter-DPU"]))
    template = template.replace("#DPU-CPU-1-vpim", str(vpim_data["1"]["DPU-CPU"]))
    
    template = template.replace("#CPU-DPU-60-native", str(native_data["60"]["CPU-DPU"]))
    template = template.replace("#DPUKernel-60-native", str(native_data["60"]["DPU"]))
    template = template.replace("#InterDPU-60-native", str(native_data["60"]["Inter-DPU"]))
    template = template.replace("#DPU-CPU-60-native", str(native_data["60"]["DPU-CPU"]))
    
    template = template.replace("#CPU-DPU-60-vpim", str(vpim_data["60"]["CPU-DPU"]))
    template = template.replace("#DPUKernel-60-vpim", str(vpim_data["60"]["DPU"]))
    template = template.replace("#InterDPU-60-vpim", str(vpim_data["60"]["Inter-DPU"]))
    template = template.replace("#DPU-CPU-60-vpim", str(vpim_data["60"]["DPU-CPU"]))

    sums = []
    for config in native_data:
        sum = 0
        for step in native_data[config]:
            sum+=native_data[config][step]
        sums.append(sum)
    for config in vpim_data:
        sum = 0
        for step in vpim_data[config]:
            sum+=vpim_data[config][step]
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
    
    overhead_1 = 0
    if sums[0]!=0:
        overhead_1 = sums[2]/sums[0]
    overhead_60 = 0
    if sums[1]!=0:
        overhead_60 = sums[3]/sums[1]
    template = template.replace("#overhead_1", str(round(overhead_1, 2)))
    template = template.replace("#overhead_60", str(round(overhead_60, 2)))
    template = template.replace("#overhead_max", str(math.ceil(max(overhead_60, overhead_1)*1.2)))
    template = template.replace("#overhead_min", str(math.floor(min(overhead_60, overhead_1)*0.8)))
    if i%4==0:
        template = template.replace("%_left", "")
    if i%4==4:
        template = template.replace("%_right", "")
    if i>11:
        template = template.replace("%_down", "")
    
    append_to_file("pgf_output", template+"\n%---\n")

    return


if __name__ == "__main__":
    if os.path.exists("pgf_output"):
        os.remove("pgf_output")
    apps = ['BS', 'TS', 'MLP', 'HST-L', 'HST-S', 'GEMV', 'VA', 'SCAN-RSS', 'SCAN-SSA', 'RED', 'SEL', 'UNI', 'SpMV', 'BFS', 'TRNS', 'NW']
    hashmap = get_data(apps, 'vPIM/')
    add_legends("pgf_output")
    for i in range(len(apps)):
        create_pfg(apps[i], hashmap, i)
        if (i+1)%4 == 0 and i!=len(apps)-1:
            append_to_file("pgf_output", "\n%---\n")    
    add_end("pgf_output")


    
    
