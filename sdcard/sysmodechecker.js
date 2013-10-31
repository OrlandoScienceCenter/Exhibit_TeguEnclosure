			// Service Mode Set
			if ((this.responseXML.getElementsByTagName('mode')[0].childNodes[0].nodeValue;) == 0)
				{
				document.getElementById("system_mode").innerHTML = "Off"
				}
			else if (this.responseXML.getElementsByTagName('mode')[0].childNodes[0].nodeValue;) == 1)
				{	
				document.getElementById("system_mode").innerHTML = "Auto"
        	       	        }
			else (this.responseXML.getElementsByTagName('mode')[0].childNodes[0].nodeValue;) == 2)
				{	
				document.getElementById("system_mode").innerHTML = "Service"
