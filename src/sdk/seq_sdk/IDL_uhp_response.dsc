STRUCT UhpResponse
{
	STRING 20	version		DEFAULT	 "1.0.0.0"
	STRING 4	errorCode
	STRING 256	errorMsg
	UINT 	8	value
	INT	4	count
	INT 	4	step
	INT	4	client_cache
	INT 	4	client_alert_diff	
}
