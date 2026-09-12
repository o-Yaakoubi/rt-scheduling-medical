Real-Time Scheduling for Medical Monitoring

FIFO vs. EDF on embedded Linux, implemented in C with POSIX threads.

Overview

In an intensive care unit, several medical sensors monitor a patient at the same time. Each sensor produces readings at its own rhythm, and each reading has a deadline by which it must be processed. When the CPU cannot keep up, the scheduler has to decide which readings to run first.

This project implements two scheduling algorithms, FIFO and EDF, in C on Ubuntu Linux using POSIX threads, and compares them under two load scenarios: a normal configuration with four sensors, and an extreme configuration with fifteen sensors that deliberately overloads the CPU.

Problem

A real-time system is not a fast system. It is a system that guarantees a response before a deadline. If the CPU is overloaded and cannot process every reading on time, the algorithm that decides the order of execution has a direct effect on patient safety.

FIFO processes readings in the order they arrive. It is simple and predictable, but it has no notion of urgency. An ECG reading due in 20 milliseconds can wait behind a temperature reading due in 200 milliseconds.

EDF always processes the reading whose deadline is closest. It behaves like hospital triage. Liu and Layland proved in 1973 that EDF is optimal on a single processor.

The question this project answers: under overload, does EDF measurably protect the most critical sensors?

Architecture

The system is a single C program with three layers.

Sensors. Each sensor is modelled with a name, a period, a deadline, an execution time and a priority. Two configurations are defined in the code. The normal set contains four sensors: ECG, SpO2, blood pressure and temperature, with a total CPU load around 75 percent. The extreme set contains fifteen sensors, with a total CPU load around 200 percent.

Job queue. A shared, thread-safe queue holds all pending readings. Each reading carries a release time, an absolute deadline and a sequence number. The scheduler selects the next job using either FIFO or EDF, chosen at runtime with a command-line argument.

CPU worker. A single consumer thread represents the CPU. It pulls one job at a time, simulates the processing time with a short sleep, and records the result in a metrics structure.

When the program ends, the metrics are printed to the terminal and written to a CSV file.

Repository contents

The src folder contains all C source files: main.c, sensor.c, sensor.h, scheduler.h, scheduler_queue.c, scheduler_fifo.c, scheduler_edf.c, metrics.c, metrics.h and the Makefile.

The report folder contains the short project report as a PDF.

Results

Two test phases were run. The numbers below come from the original experimental report.

Phase 1 - Normal load, 4 sensors, 75 percent CPU

Under normal load, both algorithms succeed completely. Every reading is processed before its deadline. FIFO completes 4756 executions with zero misses. EDF completes 4768 executions with zero misses. The choice of algorithm has no effect when the CPU has spare capacity.

Phase 2 - Extreme load, 15 sensors, 200 percent CPU

Under overload, the system cannot process everything. The two algorithms diverge clearly.

Total executions: FIFO 3399, EDF 3432
Total misses: FIFO 447, EDF 325
Success rate: FIFO 86.85 percent, EDF 90.53 percent
ECG misses: FIFO 120, EDF 98
Average ECG latency: FIFO 5.42 ms, EDF 4.70 ms

EDF reduces total misses by 122, which is a 27 percent reduction, and saves 22 ECG readings that FIFO would have dropped. Under EDF, the low-priority sensors (temperature and glucose) show zero misses, not because they are prioritised, but because they are systematically pushed to the back of the queue where they wait until the CPU has free time. This is the correct behavior for a critical-care system.

What I learned

In normal conditions, any reasonable scheduler works. The difference between algorithms only appears under deliberate overload. This is true in real-time systems, in machine learning and in reliability engineering.

The theoretical property that EDF is optimal on a single processor translates directly into measurable results. Under 200 percent load, EDF protects the vital sensors that FIFO would have abandoned.

Linux, with POSIX threads and real-time priorities, is capable of hosting a real-time medical monitoring system without requiring a dedicated real-time operating system.

How to build and run

The project requires a Linux system with GCC and the make utility. To compile:

cd rt-scheduling-medical/src
make clean
make

The Makefile also provides two shortcut targets. To run FIFO for 10 seconds on the normal sensor set:

make run-fifo

To run EDF for 10 seconds on the normal sensor set:

make run-edf

For custom durations or the extreme 15-sensor configuration, run the binary directly:

sudo ./medical_rtos --scheduler=FIFO --duration=15 --extreme --output=test_fifo.csv
sudo ./medical_rtos --scheduler=EDF  --duration=15 --extreme --output=test_edf.csv

The CSV files contain one row per sensor with the execution count, missed deadlines, miss rate and average latency.

Command-line options

--scheduler=FIFO or EDF selects the algorithm.
--duration=SECONDS sets the test duration.
--output=FILE.csv sets the output file.
--extreme enables the 15-sensor configuration instead of the default 4.
--help prints the usage message.

Limitations

Sensor readings are simulated values, not real physiological signals. The processing time is simulated with a sleep, not real computation. The system was tested in a virtual machine, so timing is looser than on real embedded hardware. The CSV result files are not included in this repository because they were generated on the original Linux test machine.

Author

Oumaima Yaakoubi
Instrumentation and Intelligent Systems, INSAT, Tunisia
LinkedIn: linkedin.com/in/oumaima-yaakoubi
GitHub: github.com/o-Yaakoubi

License

MIT License. See the LICENSE file for details.