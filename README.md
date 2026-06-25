<div align="center">
    <h1> F28HS HS-Interface </h1>
    <h3> ID#: H00460398 &nbsp;&nbsp;&nbsp;&nbsp; Name: Yuwei Zhao </h3>
</div>

---

## Labs Components Overview
| Task | Link |
| :---: | :---: |
| Traffic Lights Lab | [lab05](./src/lab05) |
| Coursework1 | [CW1](./src/coursework01) |
| Coursework2 | [CW2](./src/coursework02) |

---

## Feedback

<details>
<summary> Coursework 1 </summary>

<pre>
<code>Your program passes all of the tests in my test suite for the features you
implemented. Well done!

Q1. README.md [1/1] - Correct.

Q2. struct Image [2/2] - Correct.

Q3a. free_image [1/1] - Correct.

Q3b. load_image [2/2] - Correct.

Q3c. save_image [2/2] - Correct.

Q3d. copy_image [2/2] - Correct.

Q4. Personalised task 1 [4/4] - Correct.

Q5. Personalised task 2 [3/4]

Your program prints messages to stdout that the spec didn't ask for. This
resulted in it failing tests in my test suite that it would otherwise have
passed.

Otherwise good tidy solutions to both of those.

Q6. Multiple image files [2/2] - Correct.</code>
</pre>

</details>

<details>
<summary> Coursework 2 </summary>

<pre>
<code>Very well-presented video.

Description of implementation [2/2] - Good.

Appropriately detailed description - you went over some of this in the
video demo too.

Performance-relevant design decisions [1/2]

You talked about OS scheduling and use of timers here; you could also
discuss other performance concerns we looked at during the course, e.g.
making good use of the CPU pipeline and cache.

Work distribution and reflection [2/2] - Good.

### Code quality

C/assembler code quality [4/4]

Tidy code making good use of helper functions to make the code easier to
follow - good.

Clear function interfaces [2/2] - Correct.

Sufficient comments [2/2]

Appropriate comments in both languages.

### Functionality

Task 1: Greetings [2/2] - Correct.

Task 2: Sequence input with timeout [6/6]

Good use of named constants to make the delay design clear.

Task 2: LED and button in assembler [3/8]

You have implemented the initial computations in these functions in C rather
than assembler. We saw how to do maths operations like this (with division and
multiplication) in the ARM maths lectures.

Task 3: Hamming in assembler [5/6]

You've written a while loop in assembler that has two branches for each
iteration of the loop. This is inefficient - we saw in the lectures how to do
this with a single branch.

You've returned from the function with `pop pc`. This will work for this code,
but the reason we normally use `bx lr` is that it also switches in and out of
Thumb mode, which `pop pc` won't do.

Task 4: Simple search [2/2] - Correct.

Task 5: Extended search [5/6]

This is fine - you do end up passing a lot of arguments around
repeatedly, though! It might be better to make the less
frequently-changed ones globals or put them in a struct, and just keep
the ones that change in early args (i.e. registers).

Command-line interface, testing, performance [5/5]

Clear description of testing and performance measurement in the report.</code>
</pre>

</details>

---

#### ⚠️ License: This project now is completely open-source. See Details [LICENSE](LICENSE).