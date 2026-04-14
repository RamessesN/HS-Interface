#let table-continued = state("table-continued", false)

#let report-template(
  title: "",
  course-name: "",
  author1-name: "",
  author2-name: "",
  author1-id: "",
  author2-id: "",
  group-id: "",
  date: "",
  body
) = {
  set document(title: title, author: (author1-name, author2-name))

  set page(
    paper: "a4",
    margin: 2.5cm,
    header: context {
      if counter(page).get().first() > 1 {
        set text(size: 11pt, font: "Times New Roman")
        grid(
          columns: (1fr, auto, 1fr),
          align: (left, center, right),
          [#title],
          [#group-id],
          [#date]
        )
        v(-10pt)
        line(length: 100%, stroke: 0.6pt)
      }
    },

    footer: context {
      align(center)[
        #counter(page).display("1")
      ]
    }
  )

  set text(
    font: "Times New Roman", 
    size: 12pt,
    lang: "en",
    region: "GB"
  )

  set math.mat(delim: "[")
  set math.vec(delim: "[")
  set math.equation(numbering: "(1)")

  show ref: it => {
    let el = it.element
    if el != none and el.func() == math.equation {
      let count = counter(math.equation).at(el.location())      

      let num = numbering(el.numbering, ..count)

      set text(fill: rgb("0000FF")) 
      link(el.location(), num)
    } else {
      it
    }
  }

  show link: set text(fill: rgb("0000FF"))

  set par(
    justify: true, 
    first-line-indent: 0em, 
    spacing: 1.6em
  )

  show figure: set block(above: 0em, below: 1em)
  show figure.caption: set text(size: 10pt)
  show figure.caption: it => [
    #set text(size: 10.5pt)
    #it.supplement #context it.counter.display(it.numbering): #it.body
  ]

  set heading(numbering: "1.1")
  show heading.where(level: 1): set block(above: 2em, below: 1.2em)
  show heading.where(level: 2): set block(above: 1.2em, below: 1.2em)
  show heading.where(level: 3): set block(above: 1.2em, below: 1.2em)
  
  show raw.where(block: true): block.with(
    fill: luma(245),
    inset: 10pt,
    radius: 4pt,
    width: 100%,
  )
  show raw: set text(font: "Menlo", size: 8.5pt)

  align(center)[
    #text(weight: "bold", size: 1.6em)[#title] \
    #v(0.3em)
    #text(size: 1.2em)[#course-name] \
    #v(1em)
    #author1-name (#author1-id) #h(1em) #author2-name (#author2-id) \
    #group-id #h(1em) #date
  ]
  v(0.3cm)

  body
}

#let question(body) = {
  strong(body)
}

#let answer(body, amount: 1.2em) = {
  block(
    inset: (left: amount),
    above: 1em,
    below: 1em,
  )[
    #set par(leading: 0.6em)
    #body
  ]
}

#let code_block(fn) = (raw(lang: "matlab", block: true, read("../" + fn)))

#let sep = box(height: 1.5em)

#set math.cases(gap: 0.8em)

#let abstract(content) = {
  align(center, text(weight: "bold", size: 1.1em)[Abstract])
  pad(
    x: 2em,
    block[
      #set par(justify: false)
      #set text(size: 0.9em)
      #content
    ]
  )
}