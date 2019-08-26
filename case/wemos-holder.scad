shrinkPCT=0.6; //%, 0.2 PLA, 0.4 PETG, 0.6 ABS
fudge=.02;  //mm
shrinkFactor=1+shrinkPCT/100;
nozzleDiameter=.4;

$fn=200;

railDepth=1.5; //this is the depth of the slots that the board slides into
metalCapHeight=3.1; //the thickness of the metal cap plus the 8266 board on which it sits

boardWidth=25.9;    //width of the Wemos board
boardLength=34.3;   //length of the Wemos board
boardThickness=1.7; //thickness of the board itself, with a little extra for the groove
boardWallThickness=boardThickness; //the wall thickness at the back of the board groove
boardConnectorEndThickness=4.3;   //the thickness of the board plus the USB connector
antennaWidth=17;    //long dimension of the little PC antenna board
antennaDepth=7;     //short dimension of antenna board
antennaThickness=1; //thickness of antenna board
switchLength=6.5;   //Length of the reset switch, including the gap to the board edge
switchDepth=4.5;    //short dimension of the reset switch
switchHeight=2;     //thickness of the reset switch body
switchButtonOffset=4; //button center from front edge of card
switchButtonDiameter=1.5; //diameter of the reset button access hole
sounderDiameter=11.8;   //diameter of the sounder module
sounderHeight=8.52;    //height of the sounder module
sounderWallThickness=1; //thickness of the wall that holds the sounder in place

baseOuterDiameter=52.2; //the largest diameter of the path light base
baseThickness=1.3;      //the thickness of the plastic base plastic
baseInnerDiameter=40.7; //the diameter of the round embossment in the center of the path light base
baseInnerHeight=4.2;    //the height of the embossment above the base
baseOuterRadius=baseOuterDiameter/2; //sometimes need the radius instead of the diameter

//define a cube that will encompass the entire board. We will remove material from this cube to make the holder
boardCubeHeight=baseInnerHeight+boardConnectorEndThickness+boardThickness;
boardCubeWidth=boardWidth+boardWallThickness*2;
boardCubeLength=boardLength+boardWallThickness; //only need the extra length on the back side, front side flush

//this design works best if it is as close to the edge of the path light wall as possible.  This offset
//is how far the board cube can be moved without going over the edge of the base
boardCubeOffset=(baseOuterRadius+boardCubeLength/2)-(baseOuterRadius+sqrt(pow(baseOuterRadius,2)-pow(boardCubeWidth/2,2))); //this puts the corners of the board right at the edge of the base

boardSpaceWidth=boardWidth-railDepth*2; //the width of the remover for the board and its components 
boardSpaceLength=boardLength-railDepth; //the length of the remover for the board and its components
boardSpaceHeight=boardConnectorEndThickness+metalCapHeight; //this is how tall the remover for the wire space needs to be
boardWireSpaceWidth=boardWidth; //this is the width for the remover for the wire space
boardWireSpaceLength=20.5;      //likewise, the length of the remover
boardWireSpaceOffset=6; //from antenna end of the board


module mount()
  {
  difference()
    {
    union()
      {
      //the main cube that holds the board. Almost everything else is removed from this cube
      translate([0,boardCubeOffset,boardCubeHeight/2+baseThickness])
        {
        cube([boardCubeWidth,boardCubeLength,boardCubeHeight],center=true);
        }
        
      //Add the guide for the reset switch
      translate([-(boardWidth/2+9),
                  boardCubeOffset-(boardLength/2-switchLength/2),
                  baseThickness+baseInnerHeight+boardConnectorEndThickness-boardThickness-switchHeight/2])
        {
        rotate([0,90,0])
          cylinder(h=9,d=switchButtonDiameter+2); //hole housing for the reset button
        }
      
      //add the sounder holder
      translate([0,boardCubeOffset+boardLength/2+sounderDiameter/2+fudge,baseThickness+boardCubeHeight/2])
        {
        cylinder(h=boardCubeHeight,d=sounderDiameter+sounderWallThickness*2,center=true); //hole housing for the sounder
        }
      }
    union() //all of this is removed from the main cube
      {
      cylinder(d=baseOuterDiameter,h=baseThickness+fudge*2); //the thin base
      translate([0,0,baseThickness+fudge])
        {
        cylinder(d=baseInnerDiameter*shrinkFactor, h=baseInnerHeight*shrinkFactor); //the thicker center part of the base
        }
      translate([0,boardCubeOffset-boardWallThickness/2,baseThickness+baseInnerHeight+boardConnectorEndThickness-boardThickness/2])
        {
        cube([boardWidth+.2,boardLength+.2,boardThickness],center=true); //the slot for the card edges
        }
      translate([0,boardCubeOffset-boardWallThickness,baseThickness+baseInnerHeight+boardConnectorEndThickness-boardThickness/2])
        {
        cube([boardSpaceWidth,boardSpaceLength,boardSpaceHeight],center=true); //space for the board components
        }
      translate([0,boardCubeOffset,baseThickness+baseInnerHeight+boardConnectorEndThickness-boardThickness/2])
        {
        cube([boardWireSpaceWidth,boardWireSpaceLength,boardSpaceHeight],center=true); //space for the wire connections. This really should be changed to allow the board to be inserted with the wires attached
        }
      translate([0,boardCubeOffset-antennaDepth/2+boardLength/2,baseThickness+baseInnerHeight+boardConnectorEndThickness+antennaThickness/2-fudge])
        {
        cube([antennaWidth,antennaDepth,antennaThickness],center=true); //space for the antenna board
        }
      translate([-(boardWidth/2-switchDepth/2),boardCubeOffset-(boardLength/2-switchLength/2+boardWallThickness/2),baseThickness+baseInnerHeight+boardConnectorEndThickness-boardThickness-switchHeight/2])
        {
        cube([switchDepth+fudge,switchLength+fudge,switchHeight+fudge],center=true); //space for the reset button
        }
      translate([-(boardWidth/2+8),boardCubeOffset-(boardLength/2-switchLength/2),baseThickness+baseInnerHeight+boardConnectorEndThickness-boardThickness-switchHeight/2])
        {
        rotate([0,90,0])
          cylinder(h=8,d=switchButtonDiameter); //hole for the reset button
        }
      translate([-boardCubeWidth/2-fudge,boardCubeLength/2+boardCubeOffset-2.5,baseThickness+fudge])
        {
        cube([boardCubeWidth+fudge*2,5,baseInnerHeight]); //remover for extraneous bits left over
        }
      //add the sounder
      translate([0,boardCubeOffset+boardLength/2+sounderDiameter/2+fudge,baseThickness+boardCubeHeight/2])
        {
        cylinder(h=boardCubeHeight+fudge,d=sounderDiameter*shrinkFactor,center=true); //hole for the sounder
        }

      }
    fit();
    }
    

  }

//Create a remover ring around the whole thing to make it all fit inside the cap. This is mostly for trimming the reset button guide tube and the sounder holder
module fit()
  {
  difference()
    {
    translate([0,0,baseThickness*shrinkFactor-fudge])
      {
      cylinder(d=baseOuterDiameter+10, h=boardCubeHeight+fudge*2); 
      }
    translate([0,0,baseThickness-fudge])
      {
      cylinder(d=baseOuterDiameter, h=boardCubeHeight+fudge*2); 
      }
    }
  }
  

mount();
