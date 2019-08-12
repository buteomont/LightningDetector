shrinkPCT=.2; //%, PLA
fudge=.02;  //mm
shrinkFactor=1+shrinkPCT/100;
nozzleDiameter=.4;

$fn=200;

railDepth=1.5; //this is the depth of the slots that the board slides into
metalCapHeight=3.1; //the thickness of the metal cap plus the 8266 board on which it sits

boardWidth=25.9;
boardLength=34.3;
boardThickness=1.7;
boardWallThickness=boardThickness; //the thickness at the back of the board groove
boardConnectorEndThickness=4.3;
antennaWidth=17;
antennaDepth=7;
antennaThickness=1;
switchLength=6.5;
switchDepth=4.5;
switchHeight=2.5;
switchButtonOffset=4; //button center from front edge of card
switchButtonDiameter=2.5;

baseOuterDiameter=52.2;
baseThickness=1.3;
baseInnerDiameter=40.7;
baseInnerHeight=4.2;

boardCubeHeight=baseInnerHeight+boardConnectorEndThickness+boardThickness;
boardCubeWidth=boardWidth+boardWallThickness*2;
boardCubeLength=boardLength+boardWallThickness; //only need the extra length on the back side

baseOuterRadius=baseOuterDiameter/2;
boardCubeOffset=(baseOuterRadius+boardCubeLength/2)-(baseOuterRadius+sqrt(pow(baseOuterRadius,2)-pow(boardCubeWidth/2,2))); //this puts the corners of the board right at the edge of the base

boardSpaceWidth=boardWidth-railDepth*2;
boardSpaceLength=boardLength-railDepth;
boardSpaceHeight=boardConnectorEndThickness+metalCapHeight;
boardWireSpaceWidth=boardWidth; 
boardWireSpaceLength=20.5;
boardWireSpaceOffset=6; //from antenna end of the board

module mount()
  {
  difference()
    {
    //the main cube that holds the board
    translate([0,boardCubeOffset,boardCubeHeight/2+baseThickness])
      {
      cube([boardCubeWidth,boardCubeLength,boardCubeHeight],center=true);
      }
    union()
      {
      cylinder(d=baseOuterDiameter,h=baseThickness+fudge*2); //the thin base
      translate([0,0,baseThickness+fudge])
        {
        cylinder(d=baseInnerDiameter, h=baseInnerHeight); //the thicker center part of the base
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
        cube([boardWireSpaceWidth,boardWireSpaceLength,boardSpaceHeight],center=true); //space for the wire connections
        }
      translate([0,boardCubeOffset-antennaDepth/2+boardLength/2,baseThickness+baseInnerHeight+boardConnectorEndThickness+antennaThickness/2-fudge])
        {
        cube([antennaWidth,antennaDepth,antennaThickness],center=true); //space for the wire connections
        }
      translate([-(boardWidth/2-switchDepth/2),boardCubeOffset-(boardLength/2-switchLength/2+boardWallThickness/2),baseThickness+baseInnerHeight+boardConnectorEndThickness-boardThickness-switchHeight/2])
        {
        cube([switchDepth+fudge,switchLength+fudge,switchHeight+fudge],center=true); //space for the reset switch
        }
      translate([-(boardWidth/2+5),boardCubeOffset-(boardLength/2-switchLength/2+boardWallThickness/2),baseThickness+baseInnerHeight+boardConnectorEndThickness-boardThickness-switchHeight/2])
        {
        rotate([0,90,0])
          cylinder(h=5,d=switchButtonDiameter); //hole for the reset button
        }
      translate([-boardCubeWidth/2-fudge,boardCubeLength/2+boardCubeOffset-2.5,baseThickness+fudge])
        {
        cube([boardCubeWidth+fudge*2,5,baseInnerHeight]);
        }
      } 
    }
  }

mount();
