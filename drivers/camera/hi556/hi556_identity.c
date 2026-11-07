/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Protocol facts: A04BR3 kernel-4.4 hi556_mipi_Sensor.c return_sensor_id;
 * board Linux a04br3-hi556.c hi556_read_reg (STOP-separated transfers).
 * This implementation copies no sensor programming tables. */
#include "hi556_identity.h"
enum a04_hi556_result a04_hi556_identify(const struct a04_hi556_io *io,struct a04_hi556_observation *out)
{
 if(!out)return A04_HI556_INVALID;
 *out=(struct a04_hi556_observation){0};
#if !A04_HI556_IDENTITY_EXPERIMENT
 (void)io;return A04_HI556_DISABLED;
#else
 uint64_t lease=0;uint8_t id[2]={0};unsigned completed;
 enum a04_hi556_result result=A04_HI556_OK;
 if(!io||!io->begin||!io->live||!io->address_stop||!io->read_stop||!io->finish)return A04_HI556_INVALID;
 out->transport_status=io->begin(io->context,A04_HI556_I2C_BUS,A04_HI556_I2C_ADDRESS,&lease);
 if(out->transport_status)return A04_HI556_BEGIN_FAILED;
 for(unsigned i=0;i<2;i++){
  const uint8_t pointer[2]={0x0f,(uint8_t)(0x16+i)};
  if(!lease||io->live(io->context,lease)!=1){result=A04_HI556_LEASE_LOST;goto done;}
  out->step=i?A04_HI556_LOW_ADDRESS:A04_HI556_HIGH_ADDRESS;completed=0;
  out->transport_status=io->address_stop(io->context,lease,pointer,&completed);
  if(out->transport_status){result=A04_HI556_IO_FAILED;goto done;}
  if(completed!=2){result=A04_HI556_SHORT_IO;goto done;}
  if(io->live(io->context,lease)!=1){result=A04_HI556_LEASE_LOST;goto done;}
  out->step=i?A04_HI556_LOW_READ:A04_HI556_HIGH_READ;completed=0;
  out->transport_status=io->read_stop(io->context,lease,&id[i],&completed);
  if(out->transport_status){result=A04_HI556_IO_FAILED;goto done;}
  if(completed!=1){result=A04_HI556_SHORT_IO;goto done;}
  out->bytes_read++;
 }
 if(io->live(io->context,lease)!=1){result=A04_HI556_LEASE_LOST;goto done;}
 out->chip_id=(uint16_t)(((uint16_t)id[0]<<8)|id[1]);out->id_observed=1;
 if(out->chip_id!=A04_HI556_CHIP_ID)result=A04_HI556_ID_MISMATCH;
done:
 out->finish_status=io->finish(io->context,lease);
 if(out->finish_status&&result==A04_HI556_OK)result=A04_HI556_FINISH_FAILED;
 return result;
#endif
}
