#import "@preview/codly:1.3.0": *
#import "@preview/codly-languages:0.1.1": *

#import "lab_report_template.typ": *

#show: codly-init.with()

#codly(languages: codly-languages)

#show: report-template.with(
  title:        "CW2 Report",
  course-name:  "F28HS - Hardware-Software Interface",
  author1-name: "Shijia LUO",
  author2-name: "Yuwei ZHAO",
  author1-id:   "H00460252",
  author2-id:   "H00460398",
  group-id:     "OUC CW2 19",
  date:         "2026-04-14"
)

#abstract[
  _*Note:*_ See Resources on #link("https://gitlab-student.macs.hw.ac.uk/yz2135/f28hs-2025-26-cw2-sys").
]

// 总报告 3-5 页
= TODO1: The hardware specification and wiring (for LEDs and Button) that is used as hardware platform.

= TODO2: A description of how the work was divided between the members of the group, indicating what each student was responsiable for.

= TODO3: A short discussion of the code structure, specifying the functionality for the main functions.

= TODO4: A discussion of performance-relevant design decisions, and implications on resource consumption.

= TODO5: A list of functions directly accessing the hardware (for LEDs, Button, and LCD display) and which parts of the fuction use assembler and which use C.

= TODO6: The name and an interface discussion (what are the inputs, what is the output of the sub-routine of the Hamming function implemented in ARM Assmebler.)

= TODO7: An example set of output from the application.

= TODO8: A summary, covering that tasks have been achieved (and what not), outstanding features, and what you have learnt from this coursework.