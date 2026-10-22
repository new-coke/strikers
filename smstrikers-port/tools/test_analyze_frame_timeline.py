#!/usr/bin/env python3
"""Exercise the finite timeline parser against actual C++ writer output and corruptions."""
import csv
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from analyze_frame_timeline import analyze_sidecars

SUFFIXES=('.packets.csv','.timeline.csv','.timeline_waits.csv','.materials.csv','.views.csv')
T='.timeline.csv'
M='.materials.csv'
W='.timeline_waits.csv'
P='.packets.csv'


class TimelineTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.generated=None
        existing=os.environ.get('STRIKERS_FRAME_TIMELINE_FIXTURES')
        if existing:
            cls.source=Path(existing)/'timeline.csv'
        else:
            cls.generated=tempfile.TemporaryDirectory()
            cls.source=Path(cls.generated.name)/'timeline.csv'
            subprocess.run([os.environ['STRIKERS_PACKET_PROFILE_PROBE'],cls.generated.name],
                           check=True,capture_output=True,text=True,timeout=60)

    @classmethod
    def tearDownClass(cls):
        if cls.generated:cls.generated.cleanup()

    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.capture=Path(self.tmp.name)/'timeline.csv'
        for suffix in SUFFIXES:
            shutil.copyfile(Path(str(self.source)+suffix),Path(str(self.capture)+suffix))
        self.capture.write_text('# play_only=1 counter=live_play\n'
            'sample,match,match_frame,frame_us,tasks_us,present_us,sleep_us\n'
            '0,1,600,12000,10000,2000,0\n1,1,601,12000,10000,2000,0\n')
        self.original={s:Path(str(self.capture)+s).read_bytes() for s in SUFFIXES}

    def tearDown(self):
        self.tmp.cleanup()

    def restore(self):
        for suffix,data in self.original.items():
            Path(str(self.capture)+suffix).write_bytes(data)

    def mutate(self,suffix,change,fields_change=None):
        path=Path(str(self.capture)+suffix)
        with path.open(newline='') as file:
            metadata=file.readline()
            reader=csv.DictReader(file)
            fields=reader.fieldnames
            rows=[{k:int(v) for k,v in row.items()} for row in reader]
        change(rows)
        if fields_change:fields=fields_change(fields)
        with path.open('w',newline='') as file:
            file.write(metadata)
            writer=csv.DictWriter(file,fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)

    def reject(self,suffix,field,value,message,row=0):
        self.mutate(suffix,lambda rows:rows[row].update({field:value}))
        with self.assertRaisesRegex(ValueError,message):analyze_sidecars(self.capture)

    def remove_waits(self,index=0):
        self.mutate(T,lambda rows:rows[index].update(ready_wait_calls=0,stored_waits=0,wait_overflow=0,ready_wait_ns=0,max_ready_wait_ns=0))
        self.mutate(W,lambda rows:rows.__setitem__(slice(None),[r for r in rows if r['sample']!=index]))

    def remove_present(self,index=0):
        self.mutate(P,lambda rows:rows[index].update(owner_present_us=0))
        fields=('present_requested','present_prefix_ns','present_swap_ns','present_suffix_ns','timed_present_scopes',
                'display_add_entry_ns','display_add_entry_calls','display_add_entry_errors','display_add_entry_last_result',
                'gc_ns','gc_calls','gc_errors','gc_last_result')
        self.mutate(T,lambda rows:rows[index].update(dict.fromkeys(fields,0)))

    def remove_draws(self,index=0):
        self.remove_waits(index)
        self.mutate(P,lambda rows:rows[index].update(owner_draw_us=0))
        fields=('draw_batches','producer_draw_commands','borrowed_draw_commands','owner_draw_commands','failed_draw_commands',
                'first_draw_ready_ns','first_draw_publish_ns','first_draw_command_ns','last_draw_command_end_ns',
                'draw_prepare_ns','draw_api_ns','draw_suffix_ns','timed_draw_scopes')
        self.mutate(T,lambda rows:rows[index].update(dict.fromkeys(fields,0)))

    def command(self,*extra):
        env=dict(os.environ)
        return subprocess.run([sys.executable,str(Path(__file__).with_name('analyze_frame_timeline.py')),
                               str(self.capture),'--expected','2',*map(str,extra)],
                              capture_output=True,text=True,timeout=20,env=env)

    def test_actual_writer_and_subsets(self):
        result=analyze_sidecars(self.capture)
        self.assertEqual((result['samples'],result['stored_waits'],result['wait_overflow']),(2,64,8))
        self.assertEqual(result['provider_errors'],1)
        self.assertEqual(result['material_ns']['viewports_matrices_ns']['total'],180)
        self.assertEqual(result['material_counts']['texture_failures']['total'],1)
        self.assertEqual(result['owner_ns']['present_swap_ns']['total'],6000)
        self.assertEqual(result['frames'][0]['derived']['swap_other_ns'],2000)
        self.assertEqual(result['frames'][0]['derived']['first_draw_publish_delay_ns'],1000)
        self.assertTrue(all(f['complete_lifecycle'] for f in result['frames']))
        self.assertEqual(len(result['inputs']),4)

    def test_cli_validates_frame_and_view_identity_before_exclusive_output(self):
        output=Path(self.tmp.name)/'analysis.json'
        result=self.command('--output',output)
        self.assertEqual(result.returncode,0,result.stderr)
        saved=output.read_bytes()
        parsed=json.loads(saved)
        self.assertTrue(parsed['frame_and_packet_validation']['capture_scope']['live_play_established'])
        self.assertEqual(parsed['frame_and_packet_validation']['samples'],2)
        repeat=self.command('--output',output)
        self.assertNotEqual(repeat.returncode,0)
        self.assertEqual(output.read_bytes(),saved)

    def test_cli_frame_mismatch_never_publishes(self):
        self.capture.write_text(self.capture.read_text().replace('600,','599,'))
        output=Path(self.tmp.name)/'absent.json'
        result=self.command('--output',output)
        self.assertNotEqual(result.returncode,0)
        self.assertFalse(output.exists())

    def test_cli_explicit_nonplay_preserves_no_gameplay_claim(self):
        self.capture.write_text(self.capture.read_text().replace('# play_only=1 counter=live_play','# play_only=0 counter=renderer_scene').replace(',1,600,',',0,600,').replace(',1,601,',',0,601,'))
        self.assertNotEqual(self.command().returncode,0)
        result=self.command('--allow-nonplay')
        self.assertEqual(result.returncode,0,result.stderr)
        parsed=json.loads(result.stdout)
        self.assertFalse(parsed['frame_and_packet_validation']['capture_scope']['live_play_established'])

    def test_cli_rejects_bad_sidecar_without_output(self):
        self.mutate(M,lambda rows:rows[0].update(viewports_matrices_ns=181))
        output=Path(self.tmp.name)/'absent.json'
        self.assertNotEqual(self.command('--output',output).returncode,0)
        self.assertFalse(output.exists())

    def test_metadata(self):
        for suffix in SUFFIXES[:-1]:
            with self.subTest(suffix=suffix):
                self.restore()
                path=Path(str(self.capture)+suffix)
                path.write_bytes(path.read_bytes().replace(b'v1 ',b'v2 ',1))
                with self.assertRaisesRegex(ValueError,'metadata'):analyze_sidecars(self.capture)

    def test_columns_and_row_shape(self):
        for mutation in ('missing','extra','duplicate'):
            with self.subTest(mutation=mutation):
                self.restore()
                path=Path(str(self.capture)+T);lines=path.read_text().splitlines()
                if mutation=='missing':lines[1]=lines[1].replace('draw_batches,','')
                elif mutation=='extra':lines[2]+=',1'
                else:lines[1]=lines[1].replace('draw_batches','producer_batches')
                path.write_text('\n'.join(lines)+'\n')
                with self.assertRaisesRegex(ValueError,'columns|row'):analyze_sidecars(self.capture)

    def test_truncated_and_nonascii(self):
        for suffix in (T,M,W,P):
            for kind in ('truncated','nonascii'):
                with self.subTest(suffix=suffix,kind=kind):
                    self.restore();path=Path(str(self.capture)+suffix);data=path.read_bytes()
                    path.write_bytes(data[:-1] if kind=='truncated' else data.replace(b'sample',b'sam\xffple',1))
                    with self.assertRaisesRegex(ValueError,'incomplete|encoding'):analyze_sidecars(self.capture)

    def test_integer_domain(self):
        for field,value in (('draw_api_ns',-1),('draw_api_ns',2**64),
                            ('gc_last_result',-(2**31)-1),('gc_last_result',2**31),
                            ('owner_batches',2**32),('draw_timing_enabled',2)):
            with self.subTest(field=field,value=value):
                self.restore()
                self.reject(T,field,value,'integer|overflow|boolean')

    def test_noncanonical_integer(self):
        path=Path(str(self.capture)+T)
        for value in ('01','1.0','+1',' 1',''):
            with self.subTest(value=value):
                self.restore();lines=path.read_text().splitlines();parts=lines[2].split(',')
                parts[0]=value;lines[2]=','.join(parts);path.write_text('\n'.join(lines)+'\n')
                with self.assertRaisesRegex(ValueError,'integer'):analyze_sidecars(self.capture)

    def test_sample_counts_and_order(self):
        self.mutate(T,lambda rows:rows.pop())
        with self.assertRaisesRegex(ValueError,'sample counts'):analyze_sidecars(self.capture)
        self.restore()
        self.reject(P,'sample',1,'identity|order')

    def test_exact_identity_columns(self):
        for suffix in (T,M,W):
            for key in ('sample','live_frame','engine_frame','discarded'):
                with self.subTest(suffix=suffix,key=key):
                    self.restore()
                    self.reject(suffix,key,99,'identity|wait sample')

    def test_packet_phase_and_acceptance(self):
        self.reject(P,'material_ns',811,'phase sum')
        self.restore()
        self.reject(P,'accepted',5,'acceptance sum')

    def test_material_partition(self):
        self.reject(M,'state_lighting_ns',291,'Material children')

    def test_material_quantities(self):
        mutations={'material_attempts':5,'viewport_matrix_overrides':1,'model_concats':5,'invalid_matrices':5,
                   'point_lights':17,'directional_lights':17,'specular_lights':17,'texture_requests':25,
                   'texture_resolutions':4,'texture_failures':5}
        for field,value in mutations.items():
            with self.subTest(field=field):
                self.restore();self.reject(M,field,value,'Material attempts|counts|count|texture')

    def test_unfinished_texture_request_is_not_fabricated_failure(self):
        self.mutate(M,lambda rows:rows[0].update(texture_requests=4))
        result=analyze_sidecars(self.capture)
        self.assertEqual(result['frames'][0]['derived']['unfinished_texture_requests'],1)
        self.assertEqual(result['material_counts']['texture_failures']['total'],1)

    def test_owner_partitions(self):
        self.reject(T,'draw_suffix_ns',1001,'owner children')
        self.restore();self.reject(T,'present_suffix_ns',1001,'owner children')

    def test_timing_mode_and_scope(self):
        self.reject(T,'draw_timing_enabled',0,'timing mode|OFF')
        self.restore();self.reject(T,'timed_draw_scopes',0,'draw duration')
        self.restore();self.reject(T,'timed_present_scopes',0,'present duration')
        self.restore();self.reject(T,'timed_present_scopes',2,'multiple normal')

    def test_untimed_capture_has_absolute_events_without_children(self):
        for index in range(2):
            self.remove_present(index)
            self.mutate(P,lambda rows:rows[index].update(owner_draw_us=0))
            self.mutate(T,lambda rows:rows[index].update(draw_timing_enabled=0,draw_prepare_ns=0,draw_api_ns=0,draw_suffix_ns=0,timed_draw_scopes=0))
        result=analyze_sidecars(self.capture)
        self.assertFalse(result['timing_enabled'])
        self.assertGreater(result['derived_ns']['begin_sync_ns']['total'],0)
        self.assertEqual(result['owner_ns']['draw_api_ns']['total'],0)

    def test_present_and_discard_contract(self):
        self.reject(T,'present_requested',0,'unrequested')
        self.restore()
        for suffix in (T,P,M):
            self.mutate(suffix,lambda rows:rows[0].update(discarded=1))
        self.mutate(W,lambda rows:[row.update(discarded=1) for row in rows if row['sample']==0])
        with self.assertRaisesRegex(ValueError,'discarded frame'):analyze_sidecars(self.capture)

    def test_provider_errors_and_subsets(self):
        for field,value,message in (
            ('gc_calls',2,'provider call'),('gc_errors',2,'provider call'),
            ('gc_last_result',-1,'negative provider'),('gc_errors',1,'provider result'),
            ('gc_calls',0,'without call'),('gc_ns',3000,'subsets')):
            with self.subTest(field=field):
                self.restore();self.reject(T,field,value,message)
        self.restore()
        self.mutate(T,lambda rows:rows[0].update(gc_errors=1,gc_last_result=-(2**31)))
        self.assertEqual(analyze_sidecars(self.capture)['provider_errors'],2)

    def test_drained_counts(self):
        for field,value in (('owner_batches',4),('owner_draw_commands',2),
                            ('borrowed_draw_commands',2),('draw_batches',4),('failed_draw_commands',2)):
            with self.subTest(field=field):
                self.restore();self.reject(T,field,value,'quantities|failed Draw')

    def test_begin_and_end_order(self):
        for field,value in (('producer_begin_request_ns',0),('owner_begin_start_ns',99000),
                            ('owner_begin_end_ns',102001),('producer_end_request_ns',111001),
                            ('owner_end_end_ns',118001)):
            with self.subTest(field=field):
                self.restore();self.reject(T,field,value,'Begin|End')
        self.restore()
        self.reject(T,'producer_begin_request_ns',100000,'epoch',1)

    def test_first_draw_order(self):
        for field,value in (('first_draw_ready_ns',102999),('first_draw_publish_ns',105001),
                            ('first_draw_command_ns',109000),('last_draw_command_end_ns',111001)):
            with self.subTest(field=field):
                self.restore()
                if field=='first_draw_ready_ns':value=101999
                if field=='first_draw_command_ns':value=103999
                self.reject(T,field,value,'Draw')

    def test_no_draw_completed_frame(self):
        self.remove_draws()
        result=analyze_sidecars(self.capture)
        self.assertIsNone(result['frames'][0]['derived']['first_draw_owner_delay_ns'])
        self.assertTrue(result['frames'][0]['complete_lifecycle'])

    def test_internal_copy_scopes_need_no_queued_draw(self):
        self.remove_draws()
        self.mutate(P,lambda rows:rows[0].update(owner_draw_us=3))
        self.mutate(T,lambda rows:rows[0].update(draw_prepare_ns=1000,draw_api_ns=1000,draw_suffix_ns=1000,timed_draw_scopes=2))
        self.assertEqual(analyze_sidecars(self.capture)['frames'][0]['events_and_counts']['timed_draw_scopes'],2)

    def test_culled_command_can_have_no_timer(self):
        self.mutate(P,lambda rows:rows[0].update(owner_draw_us=0))
        self.mutate(T,lambda rows:rows[0].update(draw_prepare_ns=0,draw_api_ns=0,draw_suffix_ns=0,timed_draw_scopes=0))
        self.assertEqual(analyze_sidecars(self.capture)['frames'][0]['events_and_counts']['owner_draw_commands'],1)

    def test_failed_begin_with_known_mode_and_no_end(self):
        self.remove_draws();self.remove_present()
        self.mutate(T,lambda rows:rows[0].update(begin_succeeded=0,end_succeeded=0,producer_end_request_ns=0,producer_end_return_ns=0,
            owner_end_start_ns=0,owner_end_end_ns=0,producer_batches=1,owner_batches=1))
        result=analyze_sidecars(self.capture)
        self.assertFalse(result['frames'][0]['complete_lifecycle'])
        self.assertTrue(result['timing_enabled'])
        self.assertIsNone(result['frames'][0]['derived']['end_sync_ns'])

    def test_shutdown_cancelled_frame(self):
        self.remove_present()
        self.mutate(T,lambda rows:rows[0].update(end_succeeded=0,producer_cancelled=1,owner_cancelled=1,
            producer_end_request_ns=0,producer_end_return_ns=0,owner_end_start_ns=0,owner_end_end_ns=0))
        self.assertFalse(analyze_sidecars(self.capture)['frames'][0]['complete_lifecycle'])
        self.mutate(T,lambda rows:rows[0].update(owner_cancelled=0))
        with self.assertRaisesRegex(ValueError,'cancellation'):analyze_sidecars(self.capture)

    def test_async_error_completes_drain_without_success_claim(self):
        self.remove_present()
        self.mutate(T,lambda rows:rows[0].update(end_succeeded=0,failed_draw_commands=1))
        result=analyze_sidecars(self.capture)
        self.assertTrue(result['frames'][0]['complete_lifecycle'])
        self.assertFalse(result['frames'][0]['events_and_counts']['end_succeeded'])
        self.assertEqual(result['failed_draw_commands'],1)

    def test_missing_end_without_cancellation(self):
        self.mutate(T,lambda rows:rows[0].update(end_succeeded=0,producer_end_request_ns=0,producer_end_return_ns=0,owner_end_start_ns=0,owner_end_end_ns=0))
        with self.assertRaisesRegex(ValueError,'neither End'):analyze_sidecars(self.capture)

    def test_wait_capacity_and_overflow(self):
        for field,value in (('stored_waits',31),('wait_overflow',3),('ready_wait_calls',35)):
            with self.subTest(field=field):
                self.restore();self.reject(T,field,value,'capacity/overflow')

    def test_wait_records_must_match_count_and_identity(self):
        self.mutate(W,lambda rows:rows.pop())
        with self.assertRaisesRegex(ValueError,'capacity/overflow'):analyze_sidecars(self.capture)
        self.restore()
        self.reject(W,'sample',2,'wait sample')

    def test_wait_order_and_bounds(self):
        for field,value,row,message in (('interval',2,0,'interval ordering'),('begin_ns',104999,0,'precedes'),
                                        ('end_ns',111001,31,'past owner End'),('begin_ns',110000,1,'interval ordering'),
                                        ('end_ns',109999,0,'interval ordering')):
            with self.subTest(field=field,value=value):
                self.restore();self.reject(W,field,value,message,row)

    def test_wait_aggregates(self):
        for field,value,message in (('ready_wait_ns',159,'aggregate'),('max_ready_wait_ns',4,'aggregate'),
                                    ('ready_wait_ns',6001,'owner span')):
            with self.subTest(field=field):
                self.restore();self.reject(T,field,value,message)
        self.restore()
        self.mutate(T,lambda rows:rows[0].update(ready_wait_calls=32,wait_overflow=0,ready_wait_ns=160))
        self.assertEqual(analyze_sidecars(self.capture)['wait_overflow'],4)
        self.reject(T,'ready_wait_ns',161,'complete wait aggregate')

    def test_zero_duration_waits_are_valid(self):
        self.mutate(W,lambda rows:[row.update(end_ns=row['begin_ns']) for row in rows])
        self.mutate(T,lambda rows:[row.update(ready_wait_ns=0,max_ready_wait_ns=0) for row in rows])
        self.assertEqual(analyze_sidecars(self.capture)['owner_ns']['ready_wait_ns']['total'],0)

    def test_missing_file(self):
        Path(str(self.capture)+M).unlink()
        with self.assertRaises(OSError):analyze_sidecars(self.capture)


if __name__=='__main__':
    unittest.main(verbosity=2)
