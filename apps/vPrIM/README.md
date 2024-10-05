# vPrIM Benchmarks
We have leveraged the [PrIM Benchmarks](https://github.com/CMU-SAFARI/prim-benchmarks) to assess the performance of vPIM. During these experiments, we observed suboptimal performance when running the benchmarks with a 60 DPUs configuration. We attribute this performance issue to serial transfers and frequent small-size data transfers. We propose that the optimal workload for vPIM should exhibit the following characteristics:
<ul>
  <li>Average size for data transfer is in megabytes.</li>
  <li>Number of data transfer operations is in O(1).</li>
</ul>
To validate this assertion, we present vPrIM, a modified version of the PrIM benchmark, featuring various configurations and optimized implementations.

## Reference
Please note that our paper has not been accepted or published yet. We will provide a reference here for our work once it is available.

vPrIM builds upon the [PrIM Benchmarks](https://github.com/CMU-SAFARI/prim-benchmarks), originally introduced in the following paper:

"Juan Gómez-Luna, Izzat El Hajj, Ivan Fernandez, Christina Giannoula, Geraldo F. Oliveira, and Onur Mutlu, "[Benchmarking a New Paradigm: Experimental Analysis and Characterization of a Real Processing-in-Memory System](https://ieeexplore.ieee.org/abstract/document/9771457)". IEEE Access (2022)."

## Repository Structure
The vPrIM benchmarks maintain the same structure as the PrIM benchmarks. The benchmark folders mirror the structure of the PrIM benchmarks, as exemplified in the BFS folder:

```
.
+-- LICENSE
+-- README.md
+-- run_vprim.py
+-- pgf_output
+-- tex_template/
+-- results/
|   +--native/
|   +--vPIM/
|   +--original.json
|   +--pgf_plotter.py
+-- BFS/
|   +-- baselines/
|	|	+-- cpu/
|	|	+-- gpu/
|   +-- data/
|   +-- dpu/
|   +-- host/
|   +-- support/
|   +-- Makefile
+-- BS/
|   +-- ...
+-- GEMV/
|   +-- ...
+-- HST-L/
|   +-- ...
+-- HST-S/
|   +-- ...
+-- MLP/
|   +-- ...
+-- NW/
|   +-- ...
+-- RED/
|   +-- ...
+-- SCAN-SSA/
|   +-- ...
+-- SCAN-RSS/
|   +-- ...
+-- SEL/
|   +-- ...
+-- SpMV/
|   +-- ...
+-- TRNS/
|   +-- ...
+-- TS/
|   +-- ...
+-- UNI/
|   +-- ...
+-- VA/
|   +-- ...
```

We have made some minor modifications to the structure of the PrIM benchmarks, which include:
<ul>
  <li><b>run_vprim.py</b>: This script, based on the run_strong_rank.py script from PrIM, facilitates the execution of all vPrIM benchmarks. The output is organized within the corresponding folders in the "results/" directory. </li>
  <li><b>results/</b>: This folder serves as the repository for benchmark results. </li>
  <li><b>pgf_plotter.py</b>: Utilized for generating PGF plots, the output is stored in the "pgf_output" file based on the results from the "results/" folder.</li>
  <li><b>tex_template/</b>: Contains the LaTeX templates employed by pgf_plotter.py for generating plots.</li>
  <li><b>Microbenchmarks/</b>: This directory has been removed, as these benchmarks are not relevant within the scope of vPrIM.</li>
</ul>

## Modifications on PrIM Applications
In the context of most applications, we have primarily augmented their input data size to conform to the criterion that the average size for data transfer should be in megabytes. For other applications, we have revised their data transfer sections in their implementation by aggregating a series of small-sized data transfers into a single data transfer operation.

Here's a summary of the modifications made to individual applications:


| Application    | Input Datasize | Implemetation
| --------  | ------- | ------- |
| BS        |  262144 &rarr; 2359296                    |  NA  |
| TS        |  524288 &rarr; 16777216                   |  NA  |
| MLP       |  8192 * 1024 &rarr; 81920 * 7680          |  NA  |
| HST-L     |  1536 * 1024 &rarr; 1536 * 1024 * #DPUs   |  NA  |
| HST-S     |  1536 * 1024 &rarr; 1536 * 1024 * #DPUs   |  NA  |
| GEMV      |     |  NA  |
| VA        |     |  NA  |
| SCAN-RSS  |     |  NA  |
| SCAN-SSA  |     |  NA  |
| RED       |     |  NA  |
| SpMV      |     |  NA  |
| SEL       |     |  NA  |
| UNI       |     |  NA  |
| NW        |     |    |
| BFS       | NA  |    |
| TRNS      |     |    |

## Running vPrIM 
Please see the run_vprim.py file
