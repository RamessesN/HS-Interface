<div align="center">
    <h1> F28HS Coursework 1 </h1>
    <h3> ID#: H00460398 &nbsp;&nbsp;&nbsp;&nbsp; Name: Yuwei Zhao </h3>
</div>

---

<div align="center">
    <h2> Core Task: HQ8 BRIGHT EDGE </h2>
</div>

---

## My Tasks
- **Format:** HQ8
- **Task 1:** BRIGHT (Adjusts image brightness by a floating-point factor)
- **Task 2:** EDGE (Reports the strength of horizontal pixel differences)
- **Task 3:** Processing multiple input files

## How to Compile
To compile the program, navigate to the project directory in your terminal and run the provided makefile:

```bash
make
```

Alternatively, you can compile the source code directly using gcc:

```bash
gcc -Wall -Wextra process.c -o process
```

## How to Run
The program takes an arbitrary number of input/output image file pairs, followed by a single floating-point brightness factor at the end of the command line. The results of the `EDGE` task are printed to `stdout`.

### 1. Single Image Processing
```bash
./process <input_image> <output_image> <brightness_factor>
```

_Example (Increase brightness by 30%):_

```bash
./process input.hq8 output.hq8 1.3
```

### 2. Multiple Image Processing (Batch)
Load and process multiple images in memory by providing pairs of input and output filenames, ending with the brightness factor:

```bash
./process <in1> <out1> <in2> <out2> ... <brightness_factor>
```

_Example (Decrease brightness by 30% for two images):_

```bash
./process in1.hq8 out1.hq8 in2.hq8 out2.hq8 0.7
```