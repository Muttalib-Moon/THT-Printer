Note: This version supports only USB and Bluetooth (SPP) printing, with WiFi printing +( BLE support)
__________________________________________________________________________________________________________________________

Variable and Flag Updates:
---------------------------
The following variables and flags have been renamed for consistency:
	bool angenl_Flag = angenl_Flag
	bool angenl_PrintFlag || no-use
	bool motor_StopAngenl = motor_StopFlag
	u16 angenl_PrintWritePage = PrintWritePage_Count
	u16 angenlLength || no-use
	u8 pcount = pcont
	u16 PrintAngenlFlag = Print_start_flag
	u16 PrintAngenlPart || no-use
	u16 PrintAngenlCount || no-use
	u16 angenl_CountLine = Print_line_counter
	u16 angenl_FlagCountAdd || no-use
	u16 angenl_PageCount = Page_counter
	bool angenl_LastPage = Last_page_flag
	bool angenl_ZipFlag = still same

USB + SPP + WiFi Printing (Complete with WiFi Multi-Page Printing Issue Identified)
-----------------------------------------------------------------------------------
>Memory Optimization: The buffer size allocated for all tasks has been reduced.

>Flag Cleanup: Unused angenl flags have been removed for better efficiency.

>Buffer Channels: Separate buffer channels have been implemented for USB and SPP+wifi +BLE

>Simultaneous Printing: All chanel can now print simultaneously without issues.

>Bluetooth Exception Handling: Exceptions occurring after a Bluetooth print have been addressed.



--------------------------------------------------------------------------------------------------------------------------------------------------

<<<<<<<<----------------------------------Known Issues------------------------------------->>>>>>>

Bluetooth and USB Printing Conflict
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After a Bluetooth print session, the next USB print attempt fails and releases an empty page. On the following attempt
, it prints the end portion of the previously attempted file before completing the full print of the current file. 
This pattern occurs once per session.



