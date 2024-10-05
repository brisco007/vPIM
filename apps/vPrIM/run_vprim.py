import os 
import sys
import getpass

is_native = True
rootdir = "/home/pim/Documents/pim/vpim/apps/vPrIM" # Include path to repo
print("Root dir: " + rootdir)

applications = {"TS"       : ["NR_DPUS=X NR_TASKLETS=Y BL=Z make all", "./bin/ts_host -w 0 -n 16777216"], 
                "BS"       : ["NR_DPUS=X NR_TASKLETS=Y BL=Z make all", "./bin/bs_host -i 2359296"],
                "MLP"      : ["NR_DPUS=X NR_TASKLETS=Y BL=Z make all", "./bin/mlp_host -w 0 -e 1 -m 81920 -n 7680"],
                "HST-L"    : ["NR_DPUS=X NR_TASKLETS=Y BL=Z make all", "./bin/host_code -w 0 -e 1 -b 256 -x 2"],
                "HST-S"    : ["NR_DPUS=X NR_TASKLETS=Y BL=Z make all", "./bin/host_code -w 0 -e 1 -b 256 -x 2"],
                "NW"       : ["NR_DPUS=X NR_TASKLETS=Y BL=Z BL_IN=2 make all", "./bin/nw_host -w 0 -e 1 -n 5120"],                 
                "GEMV"     : ["NR_DPUS=X NR_TASKLETS=Y BL=Z make all", "./bin/gemv_host -w 0 -m 8192 -n 76800"],             
                "VA"       : ["NR_DPUS=X NR_TASKLETS=Y BL=Z make all", "./bin/host_code -w 0 -e 1 -i 104857600 -x 1"], 
                "SpMV"     : ["NR_DPUS=X NR_TASKLETS=Y make all", "./bin/host_code -v 0 -f data/bcsstk30.mtx.64.mtx"],
                "RED"      : ["NR_DPUS=X NR_TASKLETS=Y BL=Z VERSION=SINGLE make all", "./bin/host_code -w 0 -e 1 -i 314572800 -x 1"],
                "SCAN-RSS" : ["NR_DPUS=X NR_TASKLETS=Y BL=Z make all", "./bin/host_code -w 0 -e 1 -i 251658240 -x 1"],
                "SCAN-SSA" : ["NR_DPUS=X NR_TASKLETS=Y BL=Z make all", "./bin/host_code -w 0 -e 1 -i 251658240 -x 1"],               
                "SEL"      : ["NR_DPUS=X NR_TASKLETS=Y BL=Z make all", "./bin/host_code -w 0 -e 1 -i 125829120 -x 1"],
                "UNI"      : ["NR_DPUS=X NR_TASKLETS=Y BL=Z make all", "./bin/host_code -w 0 -e 1 -i 125829120 -x 1"],
                "BFS"      : ["NR_DPUS=X NR_TASKLETS=Y make all", "./bin/host_code -v 0 -f data/loc-gowalla_edges.txt"],
                "TRNS"     : ["NR_DPUS=X NR_TASKLETS=Y make all", "./bin/host_code -w 0 -e 1 -p 64 -o 12288 -x 1"],
                }

def run(app_name):
    
    NR_TASKLETS = [16]
    NR_DPUS = [60]
    BL = [10] 

    if app_name in applications:
        print ("------------------------ Running: "+app_name+"----------------------")
        print ("--------------------------------------------------------------------")
        if(len(applications[app_name]) > 1):
            make = applications[app_name][0]
            run_cmd = applications[app_name][1]
        
            os.chdir(rootdir + "/"+app_name)
            os.getcwd()
        
            os.system("make clean")

            try:
                os.mkdir(rootdir + "/"+ app_name +"/bin")
            except OSError:
                print ("Creation of the direction /bin failed")
                
            try:
                os.mkdir(rootdir + "/"+ app_name +"/log")
            except OSError:
                print ("Creation of the direction /log failed")
            
            try:
                os.mkdir(rootdir + "/"+ app_name +"/log/host")
            except OSError: 
                print ("Creation of the direction /log/host failed")

            try:
                os.mkdir(rootdir + "/"+ app_name +"/profile")
            except OSError:
                print ("Creation of the direction /profile failed")
            
            if app_name=='SpMV' and not os.path.exists(rootdir + "/"+app_name+"/data/bcsstk30.mtx.64.mtx"):
                os.chdir(rootdir + "/"+app_name+"/data/generate")
                os.system("./generate.sh")
                os.chdir(rootdir + "/"+app_name)
        

            for r in NR_DPUS:
                for t in NR_TASKLETS:
                    for b in BL:
                        m = make.replace("X", str(r))
                        m = m.replace("Y", str(t))
                        if (app_name == "NW"):
                            if (r == 1):
                                m = m.replace("Z", str(2560))
                            elif (r == 4):
                                m = m.replace("Z", str(640))
                            elif (r == 16):
                                m = m.replace("Z", str(160))
                            elif (r == 60):
                                m = m.replace("Z", str(40))
                        else: 
                            m = m.replace("Z", str(b))
                        print ("Running = " + m) 
                        try:
                            os.system(m)
                        except: 
                            pass 

                        r_cmd = run_cmd.replace("#ranks", str(r))
                        #r_cmd = r_cmd +  " > profile/outs_tl"+str(t)+"_bl"+str(b)+"_dpus"+str(r)
                        if is_native: 
                            r_cmd = r_cmd +  " > ../results/native/"+app_name
                        else:
                            r_cmd = r_cmd +  " > ../results/vPIM/"+app_name
                        
                        print ("Running = " + app_name + " -> "+ r_cmd)
                        try:
                            os.system(r_cmd) 
                        except:  
                            pass 
        else:
            make = applications[app_name] 

            os.chdir(rootdir + "/"+app_name)
            os.getcwd()
        
            try:
                os.mkdir(rootdir + "/"+ app_name +"/bin")
                os.mkdir(rootdir + "/"+ app_name +"/log")
                os.mkdir(rootdir + "/"+ app_name +"/log/host")
                os.mkdir(rootdir + "/"+ app_name +"/profile")
            except OSError:
                print ("Creation of the direction failed")

            print (make)    
            os.system(make + ">& profile/out")

    else:
        print ( "Application "+app_name+" not available" )

def make_clean():
    for app in applications:
        os.chdir(rootdir + "/"+app)
        os.system("make clean")
        if app == 'SpMV':
            os.remove(rootdir + "/"+app+"/data/bcsstk30.mtx.64.mtx")
            os.chdir(rootdir + "/"+app+"/data/generate")
            os.system("make clean")
        os.chdir(rootdir)


def main():
    if(len(sys.argv) < 2):
        print ("Usage: python run.py application")
        print ("Applications available: ")
        for key, value in applications.items():
            print (key )
        print ("All")

    else:
        cmd = sys.argv[1]
        print ("Application to run is: " + cmd )
        if cmd == "All":
            for key, value in applications.items():
                run(key)
                os.chdir(rootdir)
        if cmd == 'clean':
            make_clean()
        else:
            run(cmd)

if __name__ == "__main__":
    main()
