\stylesheet{

	\font[Cantarell Regular 14]
	\bggradv[#333333,#000055]
	\fgcol[#c5c5c5]

	\slidesize[1024x768]
	\ss[prestitle]{\f[1fx140u+0+0]\pad[20,20,20,20]\fontsize[64]\fgcol[#eeeeee]\center\contents}
	\ss[slidetitle]{\f[1fx90u+0+0]\pad[20,20,20,20]\fontsize[36]\fgcol[#eeeeee]\contents}
	\ss[footer]{\f[0.97fx30u+0+740]{\ralign\editable{\fontsize[11]\bold{T. A. White} | \editable{A demonstration talk | 1 March 2018} | \bold{Slide} \slidenumber}}}
	\ss[credit]{\f[600ux30u+700+700]{\fontsize[11]\contents}}
	\ss[bp]{➤ }

	\template[slide]{\slide{\slidetitle{New slide}\footer}}
	\template[bp,name="Bullet point"]{\bp{}}
	\template[credit,name="Credit"]{\credit{Image: }}

}\paraspace[0,0,10,10]\pad[10,10,10,10]\fgcol[#222222]\bgcol[#ffffff]\fontsize[40]{\bold{}Hi there, welcome to \bold{Colloquium}!}
\fontsize[16]It looks like this is the first time you've used Colloquium.  Keep reading to understand a little bit about how Colloquium works and how to use it.
Colloquium works differently to other presentation programs. Colloquium makes \italic{narrative, not slides,} the centre of attention.  Slides come when you need to illustrate something.  This window is called the \oblique{narrative editor}.  Your slides are embedded into the narrative text, like this:\slide{
\prestitle{Welcome to Colloquium}

\f[506.3ux520.3u+244.5+141.3]{\image[1fx1f+0+0,filename="colloquium.svg"]{}}\f[983.9ux75.4u+21.1+673.0]{\center This is the presentation title slide, in case you hadn't noticed.}}
As you can probably tell, the above slide happens to be the title page for the presentation.
To edit a slide, simply double-click on it.  Try it on this next slide:\slide{
\slidetitle{Here is the slide title!}
\footer{}
\f[425.5ux85.0u+519.3+526.9]{Close this window when you've finished editing...}\f[383.5ux112.1u+62.0+139.6]{Editing slides works how you expect it to.  Add a new text frame by holding shift and dragging from an empty area.  Then simply type into the new frame.}\f[442.3ux120.3u+321.6+341.8]{Shift-click and drag frames to move them.
Change their shape and size by shift-dragging the handles at the corners.}\f[0.0ux1.0u+809.1+430.0]{}}
You can add a new slide from the "Insert" menu or using the toolbar at the top of the narrative window.  Try it now: click to place the cursor at the end of this paragraph, then add a new slide.
What is the narrative window for?  Well, it's up to you!  Here are some suggestions:
\bp{}Use it just to help plan a smooth flow for your talk, reading it through to spot awkward transitions.
\bp{}Write your talk word for word.  Deliver your talk precisely as you planned it, no matter how nervous you get.
\bp{}Write bullet-pointed notes to structure your talk.
\bp{}Create a written version of your talk to print out and give to your audience as a handout.
\bp{}Use it as a journal, adding slides whenever you have an illustration.  You'll have a ready-made presentation on your activity, with no extra effort!
Besides this, Colloquium has some features which will help you when you come to give your presentation.  In the "Tools" menu, you'll find the presentation clock and the test card.
Use the test card to make sure your computer is talking to the projector correctly.  It shows you the resolution of the screen, where it thinks the edges are, and some colours.  This helps you spot and fix all-too-common display problems early.
Now, a short warning:\slide{\slidetitle{Alpha test software}\footer\f[394.9ux428.0u+571.9+178.8]{Colloquium is "alpha test" software.

It will probably crash on you and eat your work a few times.  Save and back up your work as frequently as possible.

However, in years of use it has \bold{never} crashed on me in front of an audience.  You will find most bugs in the editing features, not playback.

Creating a backup slide deck in PDF format (File->Export slides as PDF) is a wise safety measure.

Please report all bugs here:
https://www.bitwiz.me.uk/tracker}\f[452.2ux431.0u+64.8+168.9]{\image[1fx1f+0+0,filename="alpha_warning.svg"]{}}}
That's enough to get you started.  I hope you enjoy using Colloquium!
