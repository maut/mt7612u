/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

    Module Name:
    wpa.c

    Abstract:

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
    Jan Lee     03-07-22        Initial
    Rory Chen   04-11-29        Add WPA2PSK
*/
#include "rt_config.h"

extern u8 EAPOL[];

/*
    ==========================================================================
    Description:
        Port Access Control Inquiry function. Return entry's Privacy and Wpastate.
    Return:
        pEntry
    ==========================================================================
*/
MAC_TABLE_ENTRY *PACInquiry(struct rtmp_adapter *pAd, u8 Wcid)
{
    MAC_TABLE_ENTRY *pEntry = NULL;

	if (Wcid < MAX_LEN_OF_MAC_TABLE)
		pEntry = &(pAd->MacTab.Content[Wcid]);

    return pEntry;
}

/*
    ==========================================================================
    Description:
       Check sanity of multicast cipher selector in RSN IE.
    Return:
         true if match
         false otherwise
    ==========================================================================
*/
bool RTMPCheckMcast(
    IN struct rtmp_adapter *   pAd,
    IN PEID_STRUCT      eid_ptr,
    IN MAC_TABLE_ENTRY  *pEntry)
{
	u8 apidx;
	struct rtmp_wifi_dev *wdev;


	ASSERT(pEntry);
	ASSERT(pEntry->apidx < pAd->ApCfg.BssidNum);

	apidx = pEntry->apidx;

	wdev = &pAd->ApCfg.MBSSID[apidx].wdev;
	pEntry->AuthMode = wdev->AuthMode;

	if (eid_ptr->Len >= 6)
	{
        /* WPA and WPA2 format not the same in RSN_IE */
        if (eid_ptr->Eid == IE_WPA)
        {
            if (wdev->AuthMode == Ndis802_11AuthModeWPA1WPA2)
                pEntry->AuthMode = Ndis802_11AuthModeWPA;
            else if (wdev->AuthMode == Ndis802_11AuthModeWPA1PSKWPA2PSK)
                pEntry->AuthMode = Ndis802_11AuthModeWPAPSK;

            if (NdisEqualMemory(&eid_ptr->Octet[6], &pAd->ApCfg.MBSSID[apidx].RSN_IE[0][6], 4))
                return true;
        }
        else if (eid_ptr->Eid == IE_WPA2)
        {
            u8   IE_Idx = 0;

            /* When WPA1/WPA2 mix mode, the RSN_IE is stored in different structure */
            if ((wdev->AuthMode == Ndis802_11AuthModeWPA1WPA2) ||
                (wdev->AuthMode == Ndis802_11AuthModeWPA1PSKWPA2PSK))
                IE_Idx = 1;

            if (wdev->AuthMode == Ndis802_11AuthModeWPA1WPA2)
                pEntry->AuthMode = Ndis802_11AuthModeWPA2;
            else if (wdev->AuthMode == Ndis802_11AuthModeWPA1PSKWPA2PSK)
                pEntry->AuthMode = Ndis802_11AuthModeWPA2PSK;

            if (NdisEqualMemory(&eid_ptr->Octet[2], &pAd->ApCfg.MBSSID[apidx].RSN_IE[IE_Idx][2], 4))
                return true;
        }
    }

    return false;
}

/*
    ==========================================================================
    Description:
       Check sanity of unicast cipher selector in RSN IE.
    Return:
         true if match
         false otherwise
    ==========================================================================
*/
bool RTMPCheckUcast(
    IN struct rtmp_adapter *   pAd,
    IN PEID_STRUCT      eid_ptr,
    IN MAC_TABLE_ENTRY	*pEntry)
{
	u8 *	pStaTmp;
	unsigned short Count;
	u8 	apidx;
	struct rtmp_wifi_dev *wdev;

	ASSERT(pEntry);
	ASSERT(pEntry->apidx < pAd->ApCfg.BssidNum);

	apidx = pEntry->apidx;
	wdev = &pAd->ApCfg.MBSSID[apidx].wdev;

	pEntry->WepStatus = wdev->WepStatus;

	if (eid_ptr->Len < 16)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("[ERROR]RTMPCheckUcast : the length is too short(%d) \n", eid_ptr->Len));
	    return false;
	}

	/* Store STA RSN_IE capability */
	pStaTmp = (u8 *)&eid_ptr->Octet[0];
	if(eid_ptr->Eid == IE_WPA2)
	{
		/* skip Version(2),Multicast cipter(4) 2+4==6 */
		/* point to number of unicast */
        pStaTmp +=6;
	}
	else if (eid_ptr->Eid == IE_WPA)
	{
		/* skip OUI(4),Vesrion(2),Multicast cipher(4) 4+2+4==10 */
		/* point to number of unicast */
        pStaTmp += 10;
	}
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, ("[ERROR]RTMPCheckUcast : invalid IE=%d\n", eid_ptr->Eid));
	    return false;
	}

	/* Store unicast cipher count */
    memmove(&Count, pStaTmp, sizeof(unsigned short));
    Count = cpu2le16(Count);


	/* pointer to unicast cipher */
    pStaTmp += sizeof(unsigned short);

    if (eid_ptr->Len >= 16)
    {
    	if (eid_ptr->Eid == IE_WPA)
    	{
    		if (wdev->WepStatus == Ndis802_11TKIPAESMix)
			{/* multiple cipher (TKIP/CCMP) */

				while (Count > 0)
				{
					/* TKIP */
					if (MIX_CIPHER_WPA_TKIP_ON(wdev->WpaMixPairCipher))
					{
						/* Compare if peer STA uses the TKIP as its unicast cipher */
					if (RTMPEqualMemory(pStaTmp, &pAd->ApCfg.MBSSID[apidx].RSN_IE[0][12], 4))
					{
						pEntry->WepStatus = Ndis802_11TKIPEnable;
						return true;
					}

						/* Our AP uses the AES as the secondary cipher */
						/* Compare if the peer STA use AES as its unicast cipher */
						if (MIX_CIPHER_WPA_AES_ON(wdev->WpaMixPairCipher))
						{
							if (RTMPEqualMemory(pStaTmp, &pAd->ApCfg.MBSSID[apidx].RSN_IE[0][16], 4))
							{
								pEntry->WepStatus = Ndis802_11AESEnable;
								return true;
							}
						}
					}
					else
					{
					/* AES */
						if (RTMPEqualMemory(pStaTmp, &pAd->ApCfg.MBSSID[apidx].RSN_IE[0][12], 4))
					{
						pEntry->WepStatus = Ndis802_11AESEnable;
						return true;
					}
					}

					pStaTmp += 4;
					Count--;
				}
    		}
    		else
    		{/* single cipher */
    			while (Count > 0)
    			{
    				if (RTMPEqualMemory(pStaTmp , &pAd->ApCfg.MBSSID[apidx].RSN_IE[0][12], 4))
		            	return true;

					pStaTmp += 4;
					Count--;
				}
    		}
    	}
    	else if (eid_ptr->Eid == IE_WPA2)
    	{
    		u8 IE_Idx = 0;

			/* When WPA1/WPA2 mix mode, the RSN_IE is stored in different structure */
			if ((wdev->AuthMode == Ndis802_11AuthModeWPA1WPA2) || (wdev->AuthMode == Ndis802_11AuthModeWPA1PSKWPA2PSK))
				IE_Idx = 1;

			if (wdev->WepStatus == Ndis802_11TKIPAESMix)
			{/* multiple cipher (TKIP/CCMP) */

				while (Count > 0)
    			{
    				/* WPA2 TKIP */
					if (MIX_CIPHER_WPA2_TKIP_ON(wdev->WpaMixPairCipher))
					{
						/* Compare if peer STA uses the TKIP as its unicast cipher */
					if (RTMPEqualMemory(pStaTmp, &pAd->ApCfg.MBSSID[apidx].RSN_IE[IE_Idx][8], 4))
					{
						pEntry->WepStatus = Ndis802_11TKIPEnable;
						return true;
					}

						/* Our AP uses the AES as the secondary cipher */
						/* Compare if the peer STA use AES as its unicast cipher */
						if (MIX_CIPHER_WPA2_AES_ON(wdev->WpaMixPairCipher))
						{
							if (RTMPEqualMemory(pStaTmp, &pAd->ApCfg.MBSSID[apidx].RSN_IE[IE_Idx][12], 4))
							{
								pEntry->WepStatus = Ndis802_11AESEnable;
								return true;
							}
						}
					}
					else
					{
					/* AES */
						if (RTMPEqualMemory(pStaTmp, &pAd->ApCfg.MBSSID[apidx].RSN_IE[IE_Idx][8], 4))
					{
						pEntry->WepStatus = Ndis802_11AESEnable;
						return true;
					}
					}

					pStaTmp += 4;
					Count--;
				}
			}
			else
			{/* single cipher */
				while (Count > 0)
    			{
					if (RTMPEqualMemory(pStaTmp, &pAd->ApCfg.MBSSID[apidx].RSN_IE[IE_Idx][8], 4))
						return true;

					pStaTmp += 4;
					Count--;
				}
			}
    	}
    }

    return false;
}


/*
    ==========================================================================
    Description:
       Check invalidity of authentication method selection in RSN IE.
    Return:
         true if match
         false otherwise
    ==========================================================================
*/
bool RTMPCheckAKM(u8 *sta_akm, u8 *ap_rsn_ie, INT iswpa2)
{
	u8 *pTmp;
	unsigned short Count;

	pTmp = ap_rsn_ie;

	if(iswpa2)
    /* skip Version(2),Multicast cipter(4) 2+4==6 */
        pTmp +=6;
    else
    /*skip OUI(4),Vesrion(2),Multicast cipher(4) 4+2+4==10 */
        pTmp += 10;/*point to number of unicast */

    memmove(&Count, pTmp, sizeof(unsigned short));
    Count = cpu2le16(Count);

    pTmp   += sizeof(unsigned short);/*pointer to unicast cipher */

    /* Skip all unicast cipher suite */
    while (Count > 0)
    	{
		/* Skip OUI */
		pTmp += 4;
		Count--;
	}

	memmove(&Count, pTmp, sizeof(unsigned short));
    Count = cpu2le16(Count);

    pTmp   += sizeof(unsigned short);/*pointer to AKM cipher */
    while (Count > 0)
    {
		/*rtmp_hexdump(RT_DEBUG_TRACE,"MBSS WPA_IE AKM ",pTmp,4); */
		if(RTMPEqualMemory(sta_akm,pTmp,4))
		   return true;
    	else
		{
			pTmp += 4;
			Count--;
		}
    }
    return false;/* do not match the AKM */

}


/*
    ==========================================================================
    Description:
       Check sanity of authentication method selector in RSN IE.
    Return:
         true if match
         false otherwise
    ==========================================================================
*/
bool RTMPCheckAUTH(
    IN struct rtmp_adapter *   pAd,
    IN PEID_STRUCT      eid_ptr,
    IN MAC_TABLE_ENTRY	*pEntry)
{
	u8 *pStaTmp;
	unsigned short Count;
	u8 	apidx;

	ASSERT(pEntry);
	ASSERT(pEntry->apidx < pAd->ApCfg.BssidNum);

	apidx = pEntry->apidx;

	if (eid_ptr->Len < 16)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("RTMPCheckAUTH ==> WPAIE len is too short(%d) \n", eid_ptr->Len));
	    return false;
	}

	/* Store STA RSN_IE capability */
	pStaTmp = (u8 *)&eid_ptr->Octet[0];
	if(eid_ptr->Eid == IE_WPA2)
	{
		/* skip Version(2),Multicast cipter(4) 2+4==6 */
		/* point to number of unicast */
        pStaTmp +=6;
	}
	else if (eid_ptr->Eid == IE_WPA)
	{
		/* skip OUI(4),Vesrion(2),Multicast cipher(4) 4+2+4==10 */
		/* point to number of unicast */
        pStaTmp += 10;
	}
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, ("RTMPCheckAUTH ==> Unknown WPAIE, WPAIE=%d\n", eid_ptr->Eid));
	    return false;
	}

	/* Store unicast cipher count */
    memmove(&Count, pStaTmp, sizeof(unsigned short));
    Count = cpu2le16(Count);

	/* pointer to unicast cipher */
    pStaTmp += sizeof(unsigned short);

    /* Skip all unicast cipher suite */
    while (Count > 0)
    {
		/* Skip OUI */
		pStaTmp += 4;
		Count--;
	}

	/* Store AKM count */
	memmove(&Count, pStaTmp, sizeof(unsigned short));
    Count = cpu2le16(Count);

	/*pointer to AKM cipher */
    pStaTmp += sizeof(unsigned short);

    if (eid_ptr->Len >= 16)
    {
    	if (eid_ptr->Eid == IE_WPA)
    	{
			while (Count > 0)
			{
				if (RTMPCheckAKM(pStaTmp, &pAd->ApCfg.MBSSID[apidx].RSN_IE[0][0],0))
					return true;

				pStaTmp += 4;
				Count--;
			}
    	}
    	else if (eid_ptr->Eid == IE_WPA2)
    	{
    		u8 IE_Idx = 0;

			/* When WPA1/WPA2 mix mode, the RSN_IE is stored in different structure */
			if ((pAd->ApCfg.MBSSID[apidx].wdev.AuthMode == Ndis802_11AuthModeWPA1WPA2) ||
				(pAd->ApCfg.MBSSID[apidx].wdev.AuthMode == Ndis802_11AuthModeWPA1PSKWPA2PSK))
				IE_Idx = 1;

			while (Count > 0)
			{
				if (RTMPCheckAKM(pStaTmp, &pAd->ApCfg.MBSSID[apidx].RSN_IE[IE_Idx][0],1))
					return true;

				pStaTmp += 4;
				Count--;
			}
    	}
    }

    return false;
}

/*
    ==========================================================================
    Description:
       Check validity of the received RSNIE.

    Return:
		status code
    ==========================================================================
*/
UINT	APValidateRSNIE(
	IN struct rtmp_adapter *   pAd,
	IN PMAC_TABLE_ENTRY pEntry,
	IN u8 *		pRsnIe,
	IN u8 		rsnie_len)
{
	UINT StatusCode = MLME_SUCCESS;
	PEID_STRUCT  eid_ptr;
	INT	apidx;
	PMULTISSID_STRUCT pMbss;

	if (rsnie_len == 0)
		return MLME_SUCCESS;

	eid_ptr = (PEID_STRUCT)pRsnIe;
	if ((eid_ptr->Len + 2) != rsnie_len)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("[ERROR]APValidateRSNIE : the len is invalid !!!\n"));
		return MLME_UNSPECIFY_FAIL;
	}

	apidx = pEntry->apidx;
	pMbss = &pAd->ApCfg.MBSSID[apidx];


	/* check group cipher */
	if (!RTMPCheckMcast(pAd, eid_ptr, pEntry))
	{
		DBGPRINT(RT_DEBUG_ERROR, ("[ERROR]APValidateRSNIE : invalid group cipher !!!\n"));
	    StatusCode = MLME_INVALID_GROUP_CIPHER;
	}
	/* Check pairwise cipher */
	else if (!RTMPCheckUcast(pAd, eid_ptr, pEntry))
	{
		DBGPRINT(RT_DEBUG_ERROR, ("[ERROR]APValidateRSNIE : invalid pairwise cipher !!!\n"));
	    StatusCode = MLME_INVALID_PAIRWISE_CIPHER;
	}
	/* Check AKM */
	else if (!RTMPCheckAUTH(pAd, eid_ptr, pEntry))
	{
	    DBGPRINT(RT_DEBUG_ERROR, ("[ERROR]APValidateRSNIE : invalid AKM !!!\n"));
	    StatusCode = MLME_INVALID_AKMP;
	}
#ifdef DOT11W_PMF_SUPPORT
	else if (PMF_RsnCapableValidation(pAd, pRsnIe, rsnie_len,
                pMbss->PmfCfg.MFPC, pMbss->PmfCfg.MFPR, pEntry) != PMF_STATUS_SUCCESS)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("[PMF]%s : Invalid PMF Capability !!!\n", __FUNCTION__));
	    StatusCode = MLME_ROBUST_MGMT_POLICY_VIOLATION;
	}
#endif /* DOT11W_PMF_SUPPORT */

	if (StatusCode != MLME_SUCCESS)
	{
		/* send wireless event - for RSN IE sanity check fail */
		RTMPSendWirelessEvent(pAd, IW_RSNIE_SANITY_FAIL_EVENT_FLAG, pEntry->Addr, 0, 0);
		DBGPRINT(RT_DEBUG_ERROR, ("%s : invalid status code(%d) !!!\n", __FUNCTION__, StatusCode));
	}
	else
	{
        u8 CipherAlg = CIPHER_NONE;

        if (pEntry->WepStatus == Ndis802_11WEPEnabled)
            CipherAlg = CIPHER_WEP64;
        else if (pEntry->WepStatus == Ndis802_11TKIPEnable)
            CipherAlg = CIPHER_TKIP;
        else if (pEntry->WepStatus == Ndis802_11AESEnable)
            CipherAlg = CIPHER_AES;
        DBGPRINT(RT_DEBUG_TRACE, ("%s : (AID#%d WepStatus=%s)\n", __FUNCTION__, pEntry->Aid, CipherName[CipherAlg]));
	}



	return StatusCode;

}

/*
    ==========================================================================
    Description:
        Function to handle countermeasures active attack.  Init 60-sec timer if necessary.
    Return:
    ==========================================================================
*/
VOID HandleCounterMeasure(struct rtmp_adapter *pAd, MAC_TABLE_ENTRY *pEntry)
{
    INT         i;
    bool     Cancelled;

    if (!pEntry)
        return;

	/* Todo by AlbertY - Not support currently in ApClient-link */
	if (IS_ENTRY_APCLI(pEntry))
		return;

	/* if entry not set key done, ignore this RX MIC ERROR */
	if ((pEntry->WpaState < AS_PTKINITDONE) || (pEntry->GTKState != REKEY_ESTABLISHED))
		return;

	DBGPRINT(RT_DEBUG_TRACE, ("HandleCounterMeasure ===> \n"));

    /* record which entry causes this MIC error, if this entry sends disauth/disassoc, AP doesn't need to log the CM */
    pEntry->CMTimerRunning = true;
    pAd->ApCfg.MICFailureCounter++;

	/* send wireless event - for MIC error */
	RTMPSendWirelessEvent(pAd, IW_MIC_ERROR_EVENT_FLAG, pEntry->Addr, 0, 0);

    if (pAd->ApCfg.CMTimerRunning == true)
    {
        DBGPRINT(RT_DEBUG_ERROR, ("Receive CM Attack Twice within 60 seconds ====>>> \n"));

		/* send wireless event - for counter measures */
		RTMPSendWirelessEvent(pAd, IW_COUNTER_MEASURES_EVENT_FLAG, pEntry->Addr, 0, 0);
		ApLogEvent(pAd, pEntry->Addr, EVENT_COUNTER_M);

        /* renew GTK */
		GenRandom(pAd, pAd->ApCfg.MBSSID[pEntry->apidx].wdev.bssid, pAd->ApCfg.MBSSID[pEntry->apidx].GNonce);

		/* Cancel CounterMeasure Timer */
		RTMPCancelTimer(&pAd->ApCfg.CounterMeasureTimer, &Cancelled);
		pAd->ApCfg.CMTimerRunning = false;

        for (i = 0; i < MAX_LEN_OF_MAC_TABLE; i++)
        {
            /* happened twice within 60 sec,  AP SENDS disaccociate all associated STAs.  All STA's transition to State 2 */
            if (IS_ENTRY_CLIENT(&pAd->MacTab.Content[i]))
            {
                MlmeDeAuthAction(pAd, &pAd->MacTab.Content[i], REASON_MIC_FAILURE, false);
            }
        }

		/*
			Further,  ban all Class 3 DATA transportation for a period 0f 60 sec
			disallow new association , too
		*/
        pAd->ApCfg.BANClass3Data = true;

        /* check how many entry left...  should be zero */
        /*pAd->ApCfg.MBSSID[pEntry->apidx].GKeyDoneStations = pAd->MacTab.Size; */
        /*DBGPRINT(RT_DEBUG_TRACE, ("GKeyDoneStations=%d \n", pAd->ApCfg.MBSSID[pEntry->apidx].GKeyDoneStations)); */
    }

	RTMPSetTimer(&pAd->ApCfg.CounterMeasureTimer, 60 * MLME_TASK_EXEC_INTV * MLME_TASK_EXEC_MULTIPLE);
    pAd->ApCfg.CMTimerRunning = true;
    pAd->ApCfg.PrevaMICFailTime = pAd->ApCfg.aMICFailTime;
	RTMP_GetCurrentSystemTime(&pAd->ApCfg.aMICFailTime);
}


/*
    ==========================================================================
    Description:
        countermeasures active attack timer execution
    Return:
    ==========================================================================
*/
VOID CMTimerExec(
    IN PVOID SystemSpecific1,
    IN PVOID FunctionContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3)
{
    UINT            i,j=0;
    struct rtmp_adapter *  pAd = (struct rtmp_adapter *)FunctionContext;

    pAd->ApCfg.BANClass3Data = false;
    for (i = 0; i < MAX_LEN_OF_MAC_TABLE; i++)
    {
        if (IS_ENTRY_CLIENT(&pAd->MacTab.Content[i])
			&& (pAd->MacTab.Content[i].CMTimerRunning == true))
        {
            pAd->MacTab.Content[i].CMTimerRunning =false;
            j++;
        }
    }

    if (j > 1)
        DBGPRINT(RT_DEBUG_ERROR, ("Find more than one entry which generated MIC Fail ..  \n"));

    pAd->ApCfg.CMTimerRunning = false;
}


VOID WPARetryExec(
    IN PVOID SystemSpecific1,
    IN PVOID FunctionContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3)
{
    MAC_TABLE_ENTRY     *pEntry = (MAC_TABLE_ENTRY *)FunctionContext;

    if ((pEntry) && IS_ENTRY_CLIENT(pEntry))
    {
        struct rtmp_adapter *pAd = (struct rtmp_adapter *)pEntry->pAd;

        pEntry->ReTryCounter++;
        DBGPRINT(RT_DEBUG_TRACE, ("WPARetryExec---> ReTryCounter=%d, WpaState=%d \n", pEntry->ReTryCounter, pEntry->WpaState));

        switch (pEntry->AuthMode)
        {
			case Ndis802_11AuthModeWPA:
            case Ndis802_11AuthModeWPAPSK:
			case Ndis802_11AuthModeWPA2:
            case Ndis802_11AuthModeWPA2PSK:
				/* 1. GTK already retried, give up and disconnect client. */
                if (pEntry->ReTryCounter > (GROUP_MSG1_RETRY_TIMER_CTR + 1))
                {
                	/* send wireless event - for group key handshaking timeout */
					RTMPSendWirelessEvent(pAd, IW_GROUP_HS_TIMEOUT_EVENT_FLAG, pEntry->Addr, pEntry->apidx, 0);

                    DBGPRINT(RT_DEBUG_TRACE, ("WPARetryExec::Group Key HS exceed retry count, Disassociate client, pEntry->ReTryCounter %d\n", pEntry->ReTryCounter));
                    MlmeDeAuthAction(pAd, pEntry, REASON_GROUP_KEY_HS_TIMEOUT, false);
                }
				/* 2. Retry GTK. */
                else if (pEntry->ReTryCounter > GROUP_MSG1_RETRY_TIMER_CTR)
                {
                    DBGPRINT(RT_DEBUG_TRACE, ("WPARetryExec::ReTry 2-way group-key Handshake \n"));
                    if (pEntry->GTKState == REKEY_NEGOTIATING)
                    {
                        WPAStart2WayGroupHS(pAd, pEntry);
			RTMPSetTimer(&pEntry->RetryTimer, PEER_MSG3_RETRY_EXEC_INTV);
                    }
                }
				/* 3. 4-way message 1 retried more than three times. Disconnect client */
                else if (pEntry->ReTryCounter > (PEER_MSG1_RETRY_TIMER_CTR + 3))
                {
					/* send wireless event - for pairwise key handshaking timeout */
					RTMPSendWirelessEvent(pAd, IW_PAIRWISE_HS_TIMEOUT_EVENT_FLAG, pEntry->Addr, pEntry->apidx, 0);

                    DBGPRINT(RT_DEBUG_TRACE, ("WPARetryExec::MSG1 timeout, pEntry->ReTryCounter = %d\n", pEntry->ReTryCounter));
                    MlmeDeAuthAction(pAd, pEntry, REASON_4_WAY_TIMEOUT, false);
                }
				/* 4. Retry 4 way message 1, the last try, the timeout is 3 sec for EAPOL-Start */
                else if (pEntry->ReTryCounter == (PEER_MSG1_RETRY_TIMER_CTR + 3))
                {
                    DBGPRINT(RT_DEBUG_TRACE, ("WPARetryExec::Retry MSG1, the last try\n"));
                    WPAStart4WayHS(pAd , pEntry, PEER_MSG3_RETRY_EXEC_INTV);
                }
				/* 4. Retry 4 way message 1 */
                else if (pEntry->ReTryCounter < (PEER_MSG1_RETRY_TIMER_CTR + 3))
                {
                    if ((pEntry->WpaState == AS_PTKSTART) || (pEntry->WpaState == AS_INITPSK) || (pEntry->WpaState == AS_INITPMK))
                    {
                        DBGPRINT(RT_DEBUG_TRACE, ("WPARetryExec::ReTry MSG1 of 4-way Handshake\n"));
                        WPAStart4WayHS(pAd, pEntry, PEER_MSG1_RETRY_EXEC_INTV);
                    }
                }
                break;

            default:
                break;
        }
    }
#ifdef APCLI_SUPPORT
	else if ((pEntry) && IS_ENTRY_APCLI(pEntry))
	{
		if (pEntry->AuthMode == Ndis802_11AuthModeWPA || pEntry->AuthMode == Ndis802_11AuthModeWPAPSK)
		{
			struct rtmp_adapter *pAd = (struct rtmp_adapter *)pEntry->pAd;

			if (pEntry->wdev_idx < MAX_APCLI_NUM)
			{
				u8 ifIndex = pEntry->wdev_idx;

				DBGPRINT(RT_DEBUG_TRACE, ("(%s) ApCli interface[%d] startdown.\n", __FUNCTION__, ifIndex));

			}
		}
	}
#endif /* APCLI_SUPPORT */
}


/*
    ==========================================================================
    Description:
        Timer execution function for periodically updating group key.
    Return:
    ==========================================================================
*/
VOID GREKEYPeriodicExec(
    IN PVOID SystemSpecific1,
    IN PVOID FunctionContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3)
{
	UINT i, apidx;
	ULONG temp_counter = 0;
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)FunctionContext;
	PRALINK_TIMER_STRUCT pTimer = (PRALINK_TIMER_STRUCT) SystemSpecific3;
	MULTISSID_STRUCT *pMbss = NULL;
	struct rtmp_wifi_dev *wdev;

	for (apidx=0; apidx<pAd->ApCfg.BssidNum; apidx++)
	{
		if (&pAd->ApCfg.MBSSID[apidx].REKEYTimer == pTimer)
			break;
	}

	if (apidx == pAd->ApCfg.BssidNum)
		return;
	else
		pMbss = &pAd->ApCfg.MBSSID[apidx];

	wdev = &pMbss->wdev;
	if (wdev->AuthMode < Ndis802_11AuthModeWPA ||
		wdev->AuthMode > Ndis802_11AuthModeWPA1PSKWPA2PSK)
		return;

    if ((pMbss->WPAREKEY.ReKeyMethod == TIME_REKEY) && (pMbss->REKEYCOUNTER < 0xffffffff))
        temp_counter = (++pMbss->REKEYCOUNTER);
    /* REKEYCOUNTER is incremented every MCAST packets transmitted, */
    /* But the unit of Rekeyinterval is 1K packets */
    else if (pMbss->WPAREKEY.ReKeyMethod == PKT_REKEY)
        temp_counter = pMbss->REKEYCOUNTER/1000;
    else
    {
		return;
    }

	if (temp_counter > (pMbss->WPAREKEY.ReKeyInterval))
    {
        pMbss->REKEYCOUNTER = 0;
		pMbss->RekeyCountDown = 3;
        DBGPRINT(RT_DEBUG_TRACE, ("Rekey Interval Excess, GKeyDoneStations=%d\n", pMbss->StaCount));

        /* take turn updating different groupkey index, */
		if ((pMbss->StaCount) > 0)
        {
			/* change key index */
			wdev->DefaultKeyId = (wdev->DefaultKeyId == 1) ? 2 : 1;

			/* Generate GNonce randomly */
			GenRandom(pAd, wdev->bssid, pMbss->GNonce);

			/* Update GTK */
			WpaDeriveGTK(pMbss->GMK,
						(u8 *)pMbss->GNonce,
						wdev->bssid, pMbss->GTK, LEN_TKIP_GTK);

			/* Process 2-way handshaking */
            for (i = 0; i < MAX_LEN_OF_MAC_TABLE; i++)
            {
				MAC_TABLE_ENTRY  *pEntry;

				pEntry = &pAd->MacTab.Content[i];
				if (IS_ENTRY_CLIENT(pEntry) &&
					(pEntry->WpaState == AS_PTKINITDONE) &&
						(pEntry->apidx == apidx))
                {
					pEntry->GTKState = REKEY_NEGOTIATING;

#ifdef DROP_MASK_SUPPORT
					/* Disable Drop Mask */
					set_drop_mask_per_client(pAd, pEntry, 1, 0);
					set_drop_mask_per_client(pAd, pEntry, 2, 0);
#endif /* DROP_MASK_SUPPORT */

                	WPAStart2WayGroupHS(pAd, pEntry);
                    DBGPRINT(RT_DEBUG_TRACE, ("Rekey interval excess, Update Group Key for  %x %x %x  %x %x %x , DefaultKeyId= %x \n",\
										PRINT_MAC(pEntry->Addr), wdev->DefaultKeyId));
				}
			}
		}
	}

	/* Use countdown to ensure the 2-way handshaking had completed */
	if (pMbss->RekeyCountDown > 0)
	{
		pMbss->RekeyCountDown--;
		if (pMbss->RekeyCountDown == 0)
		{
			unsigned short Wcid;

			/* Get a specific WCID to record this MBSS key attribute */
			GET_GroupKey_WCID(pAd, Wcid, apidx);

			/* Install shared key table */
			WPAInstallSharedKey(pAd,
								wdev->GroupKeyWepStatus,
								apidx,
								wdev->DefaultKeyId,
								Wcid,
								true,
								pMbss->GTK,
								LEN_TKIP_GTK);
		}
	}

}


#ifdef DOT1X_SUPPORT
/*
    ========================================================================

    Routine Description:
        Sending EAP Req. frame to station in authenticating state.
        These frames come from Authenticator deamon.

    Arguments:
        pAdapter        Pointer to our adapter
        pPacket     Pointer to outgoing EAP frame body + 8023 Header
        Len             length of pPacket

    Return Value:
        None
    ========================================================================
*/
VOID WpaSend(struct rtmp_adapter *pAdapter, u8 *pPacket, ULONG Len)
{
	PEAP_HDR pEapHdr;
	u8 Addr[MAC_ADDR_LEN];
	u8 Header802_3[LENGTH_802_3];
	MAC_TABLE_ENTRY *pEntry;
	u8 *pData;


    memmove(Addr, pPacket, 6);
	memmove(Header802_3, pPacket, LENGTH_802_3);
    pEapHdr = (EAP_HDR*)(pPacket + LENGTH_802_3);
	pData = (pPacket + LENGTH_802_3);

    if ((pEntry = MacTableLookup(pAdapter, Addr)) == NULL)
    {
		return;
    }

	/* Send EAP frame to STA */
    if (((pEntry->AuthMode >= Ndis802_11AuthModeWPA) && (pEapHdr->ProType != EAPOLKey)) ||
        (pAdapter->ApCfg.MBSSID[pEntry->apidx].wdev.IEEE8021X == true))
		RTMPToWirelessSta(pAdapter,
						  pEntry,
						  Header802_3,
						  LENGTH_802_3,
						  pData,
						  Len - LENGTH_802_3,
						  (pEntry->PortSecured == WPA_802_1X_PORT_SECURED) ? false : true);


    if (RTMPEqualMemory((pPacket+12), EAPOL, 2))
    {
        switch (pEapHdr->code)
        {
			case EAP_CODE_REQUEST:
				if ((pEntry->WpaState >= AS_PTKINITDONE) && (pEapHdr->ProType == EAPPacket))
				{
					pEntry->WpaState = AS_AUTHENTICATION;
					DBGPRINT(RT_DEBUG_TRACE, ("Start to re-authentication by 802.1x daemon\n"));
				}
				break;

			/* After receiving EAP_SUCCESS, trigger state machine */
            case EAP_CODE_SUCCESS:
                if ((pEntry->AuthMode >= Ndis802_11AuthModeWPA) && (pEapHdr->ProType != EAPOLKey))
                {
                    DBGPRINT(RT_DEBUG_TRACE,("Send EAP_CODE_SUCCESS\n\n"));
                    if (pEntry->Sst == SST_ASSOC)
                    {
                        pEntry->WpaState = AS_INITPMK;
						/* Only set the expire and counters */
	                    pEntry->ReTryCounter = PEER_MSG1_RETRY_TIMER_CTR;
	                    WPAStart4WayHS(pAdapter, pEntry, PEER_MSG1_RETRY_EXEC_INTV);
                    }
                }
                else
                {
                    pEntry->PrivacyFilter = Ndis802_11PrivFilterAcceptAll;
                    pEntry->WpaState = AS_PTKINITDONE;
                    pAdapter->ApCfg.MBSSID[pEntry->apidx].wdev.PortSecured = WPA_802_1X_PORT_SECURED;
                    pEntry->PortSecured = WPA_802_1X_PORT_SECURED;
                    DBGPRINT(RT_DEBUG_TRACE,("IEEE8021X-WEP : Send EAP_CODE_SUCCESS\n\n"));
                }
                break;

            case EAP_CODE_FAILURE:
                break;

            default:
                break;
        }
    }
    else
    {
        DBGPRINT(RT_DEBUG_TRACE, ("Send Deauth, Reason : REASON_NO_LONGER_VALID\n"));
        MlmeDeAuthAction(pAdapter, pEntry, REASON_NO_LONGER_VALID, false);
    }
}


VOID RTMPAddPMKIDCache(
	IN  struct rtmp_adapter *  		pAd,
	IN	INT						apidx,
	IN	u8 *			pAddr,
	IN	u8 				*PMKID,
	IN	u8 				*PMK)
{
	INT	i, CacheIdx;

	/* Update PMKID status */
	if ((CacheIdx = RTMPSearchPMKIDCache(pAd, apidx, pAddr)) != -1)
	{
		NdisGetSystemUpTime(&(pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[CacheIdx].RefreshTime));
		memmove(&pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[CacheIdx].PMKID, PMKID, LEN_PMKID);
		memmove(&pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[CacheIdx].PMK, PMK, PMK_LEN);
		DBGPRINT(RT_DEBUG_TRACE,
					("RTMPAddPMKIDCache update %02x:%02x:%02x:%02x:%02x:%02x cache(%d) from IF(ra%d)\n",
					PRINT_MAC(pAddr), CacheIdx, apidx));

		return;
	}

	/* Add a new PMKID */
	for (i = 0; i < MAX_PMKID_COUNT; i++)
	{
		if (!pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[i].Valid)
		{
			pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[i].Valid = true;
			NdisGetSystemUpTime(&(pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[i].RefreshTime));
			COPY_MAC_ADDR(&pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[i].MAC, pAddr);
			memmove(&pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[i].PMKID, PMKID, LEN_PMKID);
			memmove(&pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[i].PMK, PMK, PMK_LEN);
			DBGPRINT(RT_DEBUG_TRACE, ("RTMPAddPMKIDCache add %02x:%02x:%02x:%02x:%02x:%02x cache(%d) from IF(ra%d)\n",
            					PRINT_MAC(pAddr), i, apidx));
			break;
		}
	}

	if (i == MAX_PMKID_COUNT)
	{
		ULONG	timestamp = 0, idx = 0;

		DBGPRINT(RT_DEBUG_TRACE, ("RTMPAddPMKIDCache(IF(%d) Cache full\n", apidx));
		for (i = 0; i < MAX_PMKID_COUNT; i++)
		{
			if (pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[i].Valid)
			{
				if (((timestamp == 0) && (idx == 0)) || ((timestamp != 0) && timestamp < pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[i].RefreshTime))
				{
					timestamp = pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[i].RefreshTime;
					idx = i;
				}
			}
		}
		pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[idx].Valid = true;
		NdisGetSystemUpTime(&(pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[idx].RefreshTime));
		COPY_MAC_ADDR(&pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[idx].MAC, pAddr);
		memmove(&pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[idx].PMKID, PMKID, LEN_PMKID);
		memmove(&pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[idx].PMK, PMK, PMK_LEN);
		DBGPRINT(RT_DEBUG_TRACE, ("RTMPAddPMKIDCache add %02x:%02x:%02x:%02x:%02x:%02x cache(%ld) from IF(ra%d)\n",
           						PRINT_MAC(pAddr), idx, apidx));
	}
}

INT RTMPSearchPMKIDCache(
	IN  struct rtmp_adapter *  pAd,
	IN	INT				apidx,
	IN	u8 *	pAddr)
{
	INT	i = 0;

	for (i = 0; i < MAX_PMKID_COUNT; i++)
	{
		if ((pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[i].Valid)
			&& MAC_ADDR_EQUAL(&pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[i].MAC, pAddr))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("RTMPSearchPMKIDCache %02x:%02x:%02x:%02x:%02x:%02x cache(%d) from IF(ra%d)\n",
            						PRINT_MAC(pAddr), i, apidx));
			break;
		}
	}

	if (i == MAX_PMKID_COUNT)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("RTMPSearchPMKIDCache - IF(%d) not found\n", apidx));
		return -1;
	}

	return i;
}

VOID RTMPDeletePMKIDCache(
	IN  struct rtmp_adapter *  pAd,
	IN	INT				apidx,
	IN  INT				idx)
{
	PAP_BSSID_INFO pInfo = &pAd->ApCfg.MBSSID[apidx].PMKIDCache.BSSIDInfo[idx];

	if (pInfo->Valid)
	{
		pInfo->Valid = false;
		DBGPRINT(RT_DEBUG_TRACE, ("RTMPDeletePMKIDCache(IF(%d), del PMKID CacheIdx=%d\n", apidx, idx));
	}
}

VOID RTMPMaintainPMKIDCache(
	IN  struct rtmp_adapter *  pAd)
{
	INT	i, j;
	ULONG Now;
	for (i = 0; i < MAX_MBSSID_NUM(pAd); i++)
	{
		PMULTISSID_STRUCT pMbss = &pAd->ApCfg.MBSSID[i];

		for (j = 0; j < MAX_PMKID_COUNT; j++)
		{
			PAP_BSSID_INFO pBssInfo = &pMbss->PMKIDCache.BSSIDInfo[j];

			NdisGetSystemUpTime(&Now);

			if ((pBssInfo->Valid)
				&& /*((Now - pBssInfo->RefreshTime) >= pMbss->PMKCachePeriod)*/
				(RTMP_TIME_AFTER(Now, (pBssInfo->RefreshTime + pMbss->PMKCachePeriod))))
			{
				RTMPDeletePMKIDCache(pAd, i, j);
			}
		}
	}
}
#endif /* DOT1X_SUPPORT */


/*
    ==========================================================================
    Description:
       Set group re-key timer

    Return:

    ==========================================================================
*/
VOID	WPA_APSetGroupRekeyAction(
	IN  struct rtmp_adapter *  pAd)
{
	UINT8	apidx = 0;

	for (apidx = 0; apidx < pAd->ApCfg.BssidNum; apidx++)
	{
		PMULTISSID_STRUCT	pMbss = &pAd->ApCfg.MBSSID[apidx];
		struct rtmp_wifi_dev *wdev = &pMbss->wdev;

		if ((wdev->WepStatus == Ndis802_11TKIPEnable) ||
			(wdev->WepStatus == Ndis802_11AESEnable) ||
			(wdev->WepStatus == Ndis802_11TKIPAESMix))
		{
			/* Group rekey related */
			if ((pMbss->WPAREKEY.ReKeyInterval != 0)
				&& ((pMbss->WPAREKEY.ReKeyMethod == TIME_REKEY) || (
					pMbss->WPAREKEY.ReKeyMethod == PKT_REKEY)))
			{
				/* Regularly check the timer */
				if (pMbss->REKEYTimerRunning == false)
				{
					RTMPSetTimer(&pMbss->REKEYTimer, GROUP_KEY_UPDATE_EXEC_INTV);

					pMbss->REKEYTimerRunning = true;
					pMbss->REKEYCOUNTER = 0;
				}
				DBGPRINT(RT_DEBUG_TRACE, (" %s : Group rekey method= %ld , interval = 0x%lx\n",
											__FUNCTION__, pMbss->WPAREKEY.ReKeyMethod,
											pMbss->WPAREKEY.ReKeyInterval));
			}
			else
				pMbss->REKEYTimerRunning = false;
		}
	}
}


#ifdef HOSTAPD_SUPPORT
/*for sending an event to notify hostapd about michael failure. */
VOID ieee80211_notify_michael_failure(
	IN	struct rtmp_adapter *   pAd,
	IN	PHEADER_802_11   pHeader,
	IN	UINT            keyix,
	IN	INT              report)
{
	static const char *tag = "MLME-MICHAELMICFAILURE.indication";
/*	struct net_device *dev = pAd->net_dev; */
/*	union iwreq_data wrqu; */
	char buf[128];		/* XXX */


	/* TODO: needed parameters: count, keyid, key type, src address, TSC */
       if(report)/*station reports a mic error to this ap. */
       {
	       snprintf(buf, sizeof(buf), "%s(keyid=%d %scast addr=%s)", tag,
		       keyix, "uni",
		       ether_sprintf(pHeader->Addr2));
       }
       else/*ap itself receives a mic error. */
       {
              snprintf(buf, sizeof(buf), "%s(keyid=%d %scast addr=%s)", tag,
		       keyix, IEEE80211_IS_MULTICAST(pHeader->Addr1) ?  "broad" : "uni",
		       ether_sprintf(pHeader->Addr2));
	 }
	RtmpOSWrielessEventSend(pAd->net_dev, RT_WLAN_EVENT_CUSTOM, -1, NULL, NULL, 0);
/*	memset(&wrqu, 0, sizeof(wrqu)); */
/*	wrqu.data.length = strlen(buf); */
/*	wireless_send_event(dev, RT_WLAN_EVENT_CUSTOM, &wrqu, buf); */
}


const CHAR* ether_sprintf(const UINT8 *mac)
{
    static char etherbuf[18];
    snprintf(etherbuf,sizeof(etherbuf),"%02x:%02x:%02x:%02x:%02x:%02x", PRINT_MAC(mac));
    return etherbuf;
}
#endif /* HOSTAPD_SUPPORT */


#ifdef APCLI_SUPPORT
#ifdef WPA_SUPPLICANT_SUPPORT
VOID    ApcliWpaSendEapolStart(
	IN	struct rtmp_adapter *pAd,
	IN  u8 *         pBssid,
	IN  PMAC_TABLE_ENTRY pMacEntry,
	IN	PAPCLI_STRUCT pApCliEntry)
{
	IEEE8021X_FRAME		Packet;
	u8               Header802_3[14];

	DBGPRINT(RT_DEBUG_TRACE, ("-----> ApCliWpaSendEapolStart\n"));

	memset(Header802_3,0, sizeof(u8)*14);

	MAKE_802_3_HEADER(Header802_3, pBssid, &pApCliEntry->wdev.if_addr[0], EAPOL);

	// Zero message 2 body
	memset(&Packet, 0, sizeof(Packet));
	Packet.Version = EAPOL_VER;
	Packet.Type    = EAPOLStart;
	Packet.Length  = cpu2be16(0);

	// Copy frame to Tx ring
	RTMPToWirelessSta((struct rtmp_adapter *)pAd, pMacEntry,
					 Header802_3, LENGTH_802_3, (u8 *)&Packet, 4, true);

	DBGPRINT(RT_DEBUG_TRACE, ("<----- WpaSendEapolStart\n"));
}
#endif /* WPA_SUPPLICANT_SUPPORT */
VOID	ApCliRTMPReportMicError(
	IN	struct rtmp_adapter *pAd,
	IN	PCIPHER_KEY 	pWpaKey,
	IN	INT			ifIndex)
{
	ULONG	Now;
	u8   unicastKey = (pWpaKey->Type == PAIRWISE_KEY ? 1:0);
	PAPCLI_STRUCT pApCliEntry = NULL;
	DBGPRINT(RT_DEBUG_TRACE, (" ApCliRTMPReportMicError <---\n"));

	pApCliEntry = &pAd->ApCfg.ApCliTab[ifIndex];
	/* Record Last MIC error time and count */
	NdisGetSystemUpTime(&Now);
	if (pAd->ApCfg.ApCliTab[ifIndex].MicErrCnt == 0)
	{
		pAd->ApCfg.ApCliTab[ifIndex].MicErrCnt++;
		pAd->ApCfg.ApCliTab[ifIndex].LastMicErrorTime = Now;
		memset(pAd->ApCfg.ApCliTab[ifIndex].ReplayCounter, 0, 8);
	}
	else if (pAd->ApCfg.ApCliTab[ifIndex].MicErrCnt == 1)
	{
		if ((pAd->ApCfg.ApCliTab[ifIndex].LastMicErrorTime + (60 * OS_HZ)) < Now)
		{
			/* Update Last MIC error time, this did not violate two MIC errors within 60 seconds */
			pAd->ApCfg.ApCliTab[ifIndex].LastMicErrorTime = Now;
		}
		else
		{

			/* RTMPSendWirelessEvent(pAd, IW_COUNTER_MEASURES_EVENT_FLAG, pAd->MacTab.Content[BSSID_WCID].Addr, BSS0, 0); */

			pAd->ApCfg.ApCliTab[ifIndex].LastMicErrorTime = Now;
			/* Violate MIC error counts, MIC countermeasures kicks in */
			pAd->ApCfg.ApCliTab[ifIndex].MicErrCnt++;
			/*
			 We shall block all reception
			 We shall clean all Tx ring and disassoicate from AP after next EAPOL frame

			 No necessary to clean all Tx ring, on RTMPHardTransmit will stop sending non-802.1X EAPOL packets
			 if pAd->StaCfg.MicErrCnt greater than 2.
			*/
		}
	}
	else
	{
		/* MIC error count >= 2 */
		/* This should not happen */
		;
	}
	MlmeEnqueue(pAd, APCLI_CTRL_STATE_MACHINE, APCLI_MIC_FAILURE_REPORT_FRAME, 1, &unicastKey, ifIndex);

	if (pAd->ApCfg.ApCliTab[ifIndex].MicErrCnt == 2)
	{
		DBGPRINT(RT_DEBUG_TRACE, (" MIC Error count = 2 Trigger Block timer....\n"));
		DBGPRINT(RT_DEBUG_TRACE, (" pAd->ApCfg.ApCliTab[%d].LastMicErrorTime = %ld\n",ifIndex,
			pAd->ApCfg.ApCliTab[ifIndex].LastMicErrorTime));

		RTMPSetTimer(&pApCliEntry->MlmeAux.WpaDisassocAndBlockAssocTimer, 100);
	}
	DBGPRINT(RT_DEBUG_TRACE, ("ApCliRTMPReportMicError --->\n"));

}
VOID ApCliWpaDisassocApAndBlockAssoc(
    IN PVOID SystemSpecific1,
    IN PVOID FunctionContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3)
{

	struct rtmp_adapter                *pAd = (struct rtmp_adapter *)FunctionContext;
	MLME_DISASSOC_REQ_STRUCT    DisassocReq;

	PAPCLI_STRUCT pApCliEntry;
	unsigned long *pCurrState = &pAd->ApCfg.ApCliTab[0].CtrlCurrState;

	pAd->ApCfg.ApCliTab[0].bBlockAssoc = true;
	DBGPRINT(RT_DEBUG_TRACE, ("(%s) disassociate with current AP after sending second continuous EAPOL frame.\n", __FUNCTION__));


	pApCliEntry = &pAd->ApCfg.ApCliTab[0];

	DisassocParmFill(pAd, &DisassocReq, pApCliEntry->MlmeAux.Bssid, REASON_MIC_FAILURE);
	MlmeEnqueue(pAd, APCLI_ASSOC_STATE_MACHINE, APCLI_MT2_MLME_DISASSOC_REQ,
		sizeof(MLME_DISASSOC_REQ_STRUCT), &DisassocReq, 0);

	pApCliEntry->MicErrCnt = 0;
}
#endif/*APCLI_SUPPORT*/
