#!/bin/perl
#

my ($fileName, $fileType) = @ARGV;
if ($fileType eq '') { $fileType = $fileName; }

my $lineSize = 16;
my $addressOffset = 0;

my ($bytes, $readCount);

open (BFH, "<$fileName") or die ("$0: error opening binary file \'$fileName\'");
binmode BFH;

while ($readCount = read(BFH, $bytes, $lineSize)) {
    print &intelHexify($bytes, $addressOffset);
    $addressOffset = $addressOffset + length($bytes);
}

close BFH;

sub intelHexify() {
    my ($bytes, $address) = @_;
    my ($sizeHex, $addressHex);
    my $byteCount = length($bytes);
    my $messageType = 0;
    my $messageTypeHex;
    my $checksumHex;
    my $checksum = 0 - $byteCount;
    $checksum = $checksum - &addressChecksum($address);
    $checksum = $checksum - ord($messageType);
    $checksum = $checksum - &bytesChecksum($bytes);
    $sizeHex        = substr("0"   . sprintf("%0X", $byteCount), -2, 2);
    $addressHex     = substr("000" . sprintf("%0X", $address), -4, 4);
    $messageTypeHex = substr("0"   . sprintf("%0X", $messageType), -2, 2);
    $checksumHex    = substr("0"   . sprintf("%0X", $checksum % 0xFF), -2 ,2);
    return (":" . $sizeHex . $addressHex . $messageTypeHex . uc(unpack("H*", $bytes) . $checksumHex . "\n"));
}

sub addressChecksum() {
    my ($address) = @_;
    my $sum = $address % 0xFF;
    return $sum + int($address / 0x100); 
}

sub bytesChecksum() {
    my ($bytes) = @_;
    my $sum = 0;
    my $count;
    for ($count = 0; $count < length($bytes); $count++) {
	$sum = $sum + ord(substr($bytes, $count, 1));
    }
    return $sum;
}
