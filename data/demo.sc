STYLES {
  NARRATIVE {
    FONT Cantarell Regular 16
    FGCOL #222222
    BGCOL #FFFFFF
    PARASPACE 0u,0u,10u,10u
    PAD 10u,10u,10u,10u
    BP {
      FONT Cantarell Regular 14
      PARASPACE 20u,0u,10u,10u
    }
    PRESTITLE {
      FONT Cantarell Bold 20
    }
  }
  SLIDE {
    GEOMETRY 1024u x 768u +0u +0u
    BGCOL VERTICAL #333333 #000055
    TEXT {
      FONT Cantarell Regular 14
      FGCOL #C5C5C5
      BGCOL 0.00,0.00,0.00,0.00
      PARASPACE 5u,5u,5u,5u
      PAD 0u,0u,0u,0u
    }
    PRESTITLE {
      GEOMETRY 1f x 140u +0u +0u
      FONT Cantarell Regular 64
      FGCOL #EEEEEE
      BGCOL 0.00,0.00,0.00,0.00
      PAD 20u,20u,20u,20u
      ALIGN center
    }
    SLIDETITLE {
      GEOMETRY 1f x 90u +0u +0u
      FONT Cantarell Regular 36
      FGCOL #EEEEEE
      BGCOL 0.00,0.00,0.00,0.00
      PAD 20u,20u,20u,20u
    }
    FOOTER {
      GEOMETRY 1f x 35u +0u +738u
      FGCOL 1.00,1.00,1.00,1.00
      BGCOL 0.76,0.49,0.07,1.00
      PARASPACE 0u,0u,0u,0u
      PAD 0u,0u,0u,0u
      ALIGN right
    }
  }
}


PRESTITLE: Hi there, welcome to Colloquium!
: It looks like this is the first time you've used Colloquium.  Keep reading to understand a little bit about how Colloquium works and how to use it.
: 
: Colloquium works differently to other presentation programs. Colloquium makes /narrative, not slides,/ the centre of attention.  Slides come when you need to illustrate something.  This window is called the /narrative editor/.  Your slides are embedded into the narrative text, like this:
SLIDE {
  PRESTITLE: Welcome to Colloquium
  IMAGE[506.3ux520.3u+244.5u+141.3u]: colloquium.svg
  TEXT[983.9ux75.4u+21.1u+673u][center]: This is the presentation title slide, in case you hadn't noticed.
}
: As you can probably tell, the above slide happens to be the title page for the presentation.
: To edit a slide, simply double-click on it.  Try it on this next slide:
SLIDE {
  SLIDETITLE: Here is the slide title!
  FOOTER
  TEXT[425.5ux85u+519.3u+526.9u]: Close this window when you've finished editing...
  TEXT[383.5ux112.1u+62u+139.6u]: Editing slides works how you expect it to.  Add a new text frame by holding shift and dragging from an empty area.  Then simply type into the new frame.
  TEXT[442.3ux120.3u+321.6u+341.8u]: Shift-click and drag frames to move them.
                                   : Change their shape and size by shift-dragging the handles at the corners.
}
: You can add a new slide from the "Insert" menu or using the toolbar at the top of the narrative window.  Try it now: click to place the cursor at the end of this paragraph, then add a new slide.
: What is the narrative window for?  Well, it's up to you!  Here are some suggestions:
BP: Use it just to help plan a smooth flow for your talk, reading it through to spot awkward transitions.
BP: Write your talk word for word.  Deliver your talk precisely as you planned it, no matter how nervous you get.
BP: Write bullet-pointed notes to structure your talk.
BP: Create a written version of your talk to print out and give to your audience as a handout.
BP: Use it as a journal, adding slides whenever you have an illustration.  You'll have a ready-made presentation on your activity, with no extra effort!
: Besides this, Colloquium has some features which will help you when you come to give your presentation.  In the "Tools" menu, you'll find the presentation clock and the test card.
: Use the test card to make sure your computer is talking to the projector correctly.  It shows you the resolution of the screen, where it thinks the edges are, and some colours.  This helps you spot and fix all-too-common display problems early.
: Now, a short warning:
SLIDE {
  SLIDETITLE: Alpha test software
  FOOTER
  TEXT[391.8ux473.5u+567.8u+143.7u]: Colloquium is "alpha test" software.
                                   : 
                                   : It will probably crash and lose your work a few times.  Save and back up your work as frequently as possible.
                                   : 
                                   : However, in years of use it has *never* crashed on me in front of an audience.  You will find most bugs in the editing features, not playback.
                                   : 
                                   : Creating a backup slide deck in PDF format (File->Export slides as PDF) is nevertheless a wise safety measure.
                                   : 
                                   : Please report all bugs here:
                                   : https://github.com/taw10/colloquium/issues
  IMAGE[452.2ux431u+64.8u+168.9u]: alpha_warning.svg
}
: That's enough to get you started.  I hope you enjoy using Colloquium!
