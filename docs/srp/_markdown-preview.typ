#import "@preview/cmarker:0.1.8"

#include "/documentation/specifications/srp.typ"

= Submission Instructions
(In the following, replace LABEL with EDF, SRP, CBS, or MP for each of the four tasks.)

== Changes.
In a markdown file with the name `changes_LABEL.md`, record all of the changes (and additions) that you made to FreeRTOS in order to support the functionality in every task. Make sure to include all the files that you altered as well as the functions you changed, and also list the new functions that you added.

== Design.
Create a markdown document named `design_LABEL.md`. In this document, include all your design choices, including brief explanation of how you implemented partitioned and global EDF. Detail the feasibility tests you used in each. Flowcharts are nice here!

== Testing.
Create a markdown document named `testing_LABEL.md`. For every task, include the general testing methodology you used, in addition to all the test cases. For each test case, provide an explanation as to the specific functionality/scenario that it tests. Also indicate the result of each test case.

== Bugs.
Create a file named `bugs_LABEL.md` and include a list of the current bugs in your implementations.

== Future improvements.
Create a file named `future_LABEL.md` and include a list of the things you could do to improve your implementations, have you had the time to do them. This includes optimizations, decision choices, and basically anything you deemed lower priority `TODO`.

// This part reads the markdown files listed, and parses the markdown into valid typst before outputting each file starting on a fresh page
#let preview-files = (
  "changes_SRP.md",
  "testing_SRP.md",
  "bugs_SRP.md",
  "future_SRP.md",
  "design_SRP.md",
)
#for file in preview-files {
  let file_contents = read("/documentation/srp/" + file)
  if file_contents.len() > 0 {
    pagebreak()
    cmarker.render(
      file_contents,
      scope: (image: (source, alt: none, format: auto) => image(source, alt: alt, format: format))
    )
  }
}
