##########################################################################################
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape 
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 2/10/00 attinasi
#
##########################################################################################

#############################################
# User-defined variables
#
$machineStats = "WinNT 4.0 (sp5), 450 MHz, 128mg RAM";
#
#############################################

sub debug_print {
  foreach $str (@_){
#    print( $str );
  }
}

@ARGV;
$buildRoot = $ARGV[0];
$buildIDFile = '< '.$buildRoot.'\bin\chrome\navigator\locale\navigator.dtd';
$pullDate = $ARGV[1];
$useClockTime = $ARGV[2];

open (XUL_FILE, $buildIDFile) or die "Unable to open BuildID file $buildIDFile (header.pl)";
$BuildNo = "";
$LineList;
while (<XUL_FILE>)
{
  $ThisLine = $_;
  chop ($ThisLine);
  if (/Build ID/){
    @LineList = split (/\"/, $ThisLine);
    $BuildNo = $LineList[1];
  }
}
$BuildNo =~ s/"//g;
$BuildNo =~ s/[>]//g;
close (XUL_FILE);
debug_print ($BuildNo);

#############################################

($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst)=localtime;
%weekday= (
  "1", "$day",
  '2', 'Tuesday',
  '3', 'Wednesday',
  '4', 'Thursday',
  '5', 'Friday',
  '6', 'Saturday',
  '7', 'Sunday',
);
$mon += 1;
$year += 1900;

open (TABLE_FILE, ">table.html");

print (TABLE_FILE "<center><b><font size=+2>Top 40 Sites - File Load Performance Metrics</font></b></center>");
print (TABLE_FILE "<center><b><font size=+2>Seamonkey Win32</font></B></Center>");
print (TABLE_FILE "<BR>");
print (TABLE_FILE "<center><font size=+2 color=maroon>$pullDate  / $BuildNo </font></center>");
print (TABLE_FILE "<BR><center><b><font size=+1>");
print (TABLE_FILE "$weekday{$wday} ");
print (TABLE_FILE "$mon/$mday/$year ");
printf (TABLE_FILE "%02d:%02d:%02d", $hour, $min, $sec);
print (TABLE_FILE "</font></b></center>");
print (TABLE_FILE "<BR>");
print (TABLE_FILE "<B><CENTER><font size=-1>\n");
print (TABLE_FILE "$machineStats\n"); 
print (TABLE_FILE "<BR>");
if($useClockTime){
  print (TABLE_FILE "Time is reported in Seconds of Clock time");
} else {
  print (TABLE_FILE "Time is reported in Seconds of CPU time");
}
print (TABLE_FILE "</font></CENTER></B>\n");
print (TABLE_FILE "<BR>\n\n");

print (TABLE_FILE "<table BORDER COLS=15 WIDTH='90%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<tr>");
print (TABLE_FILE "<td WIDTH='25%'></td>");
print (TABLE_FILE "<td COLSPAN='2' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Parsing</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='2' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Content Creation</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='2' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Frame Creation</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='2' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Style Resolution</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='2' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Reflow</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='2' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Total Layout Time</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='2' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Total Page Load Time</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "</tr>");
print (TABLE_FILE "<tr>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Sites</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Time</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>%</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Time</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>%</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Time</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>%</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Time</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>%</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Time</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>%</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Time</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>%</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "<td COLSPAN='1' WIDTH='25%' BGCOLOR='#CCFFFF'>");
print (TABLE_FILE "<center><b>Time</b></center>");
print (TABLE_FILE "</td>");
print (TABLE_FILE "</tr>\n\n");
close (TABLE_FILE);
