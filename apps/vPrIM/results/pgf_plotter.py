import numpy as np
import sys
import os
import re
import pandas as pd
import math
import json

def read_entries(path):
    entries = {'CPU-DPU': 0, 'DPU': 0, 'Inter-DPU': 0, 'DPU-CPU': 0}
    try:
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
    
def get_vprim_data(apps):
    hashmap = {'native': {}, 'VPIM': {}}
    for app in apps:
        hashmap['native'][app] = read_entries('results/native/'+app)
        hashmap['VPIM'][app] = read_entries('results/vPIM/'+app)
    return hashmap

def get_prim_data():
    json_file = open('results/original.json')
    hashmap = json.load(json_file)
    json_file.close()
    return hashmap

def collect_data(apps):
    hashmap_vprim = get_vprim_data(apps)
    hashmap_prim = get_prim_data()
    hashmap = {'native':{}, 'VPIM':{}}
    for app in apps:
        hashmap['native'][app] = {'prim':{}, 'vprim': {}}
        hashmap['VPIM'][app] = {'prim':{}, 'vprim': {}}
        hashmap['native'][app]['prim'] = hashmap_prim['native'][app]
        hashmap['native'][app]['vprim'] = hashmap_vprim['native'][app]
        hashmap['VPIM'][app]['prim'] = hashmap_prim['VPIM'][app]
        hashmap['VPIM'][app]['vprim'] = hashmap_vprim['VPIM'][app]
    return hashmap

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

def create_pfg(type, hashmap, i):
    native_data = hashmap["native"][type]
    vpim_data = hashmap["VPIM"][type]
    template = get_template()

    template = template.replace("#CPU-DPU-prim-native", str(native_data["prim"]["CPU-DPU"]))
    template = template.replace("#DPUKernel-prim-native", str(native_data["prim"]["DPU"]))
    template = template.replace("#InterDPU-prim-native", str(native_data["prim"]["Inter-DPU"]))
    template = template.replace("#DPU-CPU-prim-native", str(native_data["prim"]["DPU-CPU"]))
    
    template = template.replace("#CPU-DPU-prim-vpim", str(vpim_data["prim"]["CPU-DPU"]))
    template = template.replace("#DPUKernel-prim-vpim", str(vpim_data["prim"]["DPU"]))
    template = template.replace("#InterDPU-prim-vpim", str(vpim_data["prim"]["Inter-DPU"]))
    template = template.replace("#DPU-CPU-prim-vpim", str(vpim_data["prim"]["DPU-CPU"]))
    
    template = template.replace("#CPU-DPU-vprim-native", str(native_data["vprim"]["CPU-DPU"]))
    template = template.replace("#DPUKernel-vprim-native", str(native_data["vprim"]["DPU"]))
    template = template.replace("#InterDPU-vprim-native", str(native_data["vprim"]["Inter-DPU"]))
    template = template.replace("#DPU-CPU-vprim-native", str(native_data["vprim"]["DPU-CPU"]))
    
    template = template.replace("#CPU-DPU-vprim-vpim", str(vpim_data["vprim"]["CPU-DPU"]))
    template = template.replace("#DPUKernel-vprim-vpim", str(vpim_data["vprim"]["DPU"]))
    template = template.replace("#InterDPU-vprim-vpim", str(vpim_data["vprim"]["Inter-DPU"]))
    template = template.replace("#DPU-CPU-vprim-vpim", str(vpim_data["vprim"]["DPU-CPU"]))

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
    apps = ['BS', 'TS', 'MLP', 'HST-L', 'HST-S', 'GEMV', 'VA', 'SCAN-RSS', 'SCAN-SSA', 'RED', 'SEL', 'UNI', 'SpMV', 'NW', 'BFS', 'TRNS']
    hashmap = collect_data(apps)
    add_legends("pgf_output")
    for i in range(len(apps)):
        create_pfg(apps[i], hashmap, i)
        if (i+1)%4 == 0 and i!=len(apps)-1:
            append_to_file("pgf_output", "\n%---\n")    
    add_end("pgf_output")

    
    
