// SPDX-License-Identifier: GPL-3.0-or-later
// Native Messaging framing and two browser-adapter simulations (NO physical hardware).
import assert from 'node:assert/strict';
import {readFileSync, existsSync, mkdtempSync, rmSync} from 'node:fs';
import {resolve,join} from 'node:path';
import {tmpdir} from 'node:os';
import {spawnSync} from 'node:child_process';
import {runInNewContext} from 'node:vm';

const buildDir=process.env.BRIDGE_CI_BUILD_DIR??'.ci-build/native';
const configuration=process.env.BRIDGE_CI_CONFIGURATION??'Debug';
const binary=resolve(process.platform==='win32'
  ? buildDir+'/'+configuration+'/lagosystem-bridge.exe'
  : buildDir+'/lagosystem-bridge');
assert.ok(existsSync(binary),'Host binary not found: '+binary);
const home=mkdtempSync(join(tmpdir(),'bridge-ci-'));
const channel='local-device-bridge-v1';
let calls=0;

function native(request, allowElevatedRefusal = false) {
  calls++;
  const body=Buffer.from(JSON.stringify(request),'utf8');
  const header=Buffer.alloc(4);
  header.writeUInt32LE(body.length);
  const response=spawnSync(binary,[],{
    input:Buffer.concat([header,body]),encoding:'buffer',
    timeout:8000,maxBuffer:2*1024*1024,
    env:{...process.env,HOME:home,USERPROFILE:home,LOCALAPPDATA:home}
  });
  if(response.error) throw response.error;
  if (allowElevatedRefusal && process.platform === 'win32' && response.status === 3) {
    assert.match(response.stderr?.toString() ?? '', /refuses elevated\/root execution/,
      'Windows elevated-process denial should be explicit and safe');
    return {elevatedRefusal: true};
  }
  const exitHex = (response.status >>> 0).toString(16).toUpperCase().padStart(8, '0');
  assert.equal(response.status, 0,
    'Native process failed (exit 0x' + exitHex + ', decimal ' + response.status +
    '): ' + response.stderr?.toString());
  assert.ok(response.stdout.length>=4,'Missing native response length');
  const count=response.stdout.readUInt32LE(0);
  assert.ok(count>0 && count<=1048576);
  assert.equal(response.stdout.length,count+4,'Native frame length mismatch');
  return JSON.parse(response.stdout.subarray(4).toString('utf8'));
}

function extension(family) {
  const state={approvedOrigins:[],perOriginDevices:{}};
  let handler;
  const runtime={
    onMessage:{addListener:listener=>{handler=listener;}},
    sendNativeMessage:async(host,request)=>{
      assert.equal(host,'com.lagosystem.bridge');return native(request);
    }
  };
  const local={
    get:async key=>({[key]:state[key]}),
    set:async updates=>Object.assign(state,updates)
  };
  const code=readFileSync(resolve('extensions',family,'background.js'),'utf8');
  runInNewContext(code,{chrome:{runtime,storage:{local}},URL,console},
    {filename:family+'/background.js'});
  assert.equal(typeof handler,'function');
  const send=(url,action,fields={})=>new Promise((done,fail)=>{
    const timeout=setTimeout(()=>fail(new Error('Bridge background timed out')),5000);
    try{
      assert.equal(handler({channel,action,...fields},{url},value=>{
        clearTimeout(timeout);done(value);
      }),true);
    }catch(error){clearTimeout(timeout);fail(error);}
  });
  return {state,send};
}

async function content(family,background) {
  const origin='https://approved.example';
  let listener;
  const output=[];
  const window={
    addEventListener:(kind,callback)=>{assert.equal(kind,'message');listener=callback;},
    postMessage:(payload,target)=>{assert.equal(target,origin);output.push(payload);}
  };
  const code=readFileSync(resolve('extensions',family,'content.js'),'utf8');
  runInNewContext(code,{
    chrome:{runtime:{sendMessage:message=>background.send(origin+'/page',message.action,message)}},
    window,location:{origin},console
  },{filename:family+'/content.js'});
  const msg=(id,action)=>({channel,direction:'request',requestId:id,action});
  listener({source:window,origin,data:msg('detect','detect')});
  assert.equal(output.at(-1).payload.extensionInstalled,true);
  const count=output.length;
  listener({source:window,origin:'https://wrong.example',data:msg('forged','ping')});
  assert.equal(output.length,count,'Cross-origin page message was accepted');
  listener({source:window,origin,data:msg('ping','ping')});
  for(let n=0;n<100&&output.length===count;n++)
    await new Promise(resolveTick=>setTimeout(resolveTick,20));
  assert.equal(output.length,count+1,'No content-script response');
  assert.equal(output.at(-1).requestId,'ping');
  assert.equal(output.at(-1).payload.protocolVersion,1);
}

try {
  const first = native({action:'ping'}, true);
  if (first.elevatedRefusal) {
    // GitHub-hosted Windows runners run with an elevated administrator token.
    // The production C++ host MUST continue to fail closed instead of allowing
    // native device operations as administrator. Other platforms exercise the
    // full real-host + browser adapter integration with an unprivileged token.
    console.log('Windows host correctly refused elevated execution (exit 3).');
    console.log('Skipping native IPC in elevated Windows CI; C++ fixtures and both');
    console.log('browser manifest/source checks already ran; Linux/macOS test real IPC.');
  } else {
  assert.equal(first.protocolVersion,1);
  assert.equal(native({action:'does.not.exist'}).error,'unknown_or_unpaired_operation');
  for(const family of ['chromium','firefox']) {
    const bridge=extension(family);
    const allowed='https://approved.example',denied='https://unapproved.example';
    const before=calls;
    assert.equal((await bridge.send(denied+'/page','ping')).error,'origin_not_approved');
    assert.equal(calls,before,'Unapproved origin accessed native host');
    bridge.state.approvedOrigins=[allowed];
    assert.equal((await bridge.send(denied+'/page','ping',{origin:allowed})).error,
      'origin_not_approved','Browser sender origin must override untrusted payload');
    assert.equal((await bridge.send(allowed+'/page','shell.exec')).error,'invalid_request');
    assert.equal(calls,before,'Unauthorized action reached native host');
    const pong=await bridge.send(allowed+'/page','ping');
    assert.equal(pong.ok,true);assert.equal(pong.protocolVersion,1);
    const caps=await bridge.send(allowed+'/page','capabilities');
    assert.equal(caps.ok,true);assert.ok(caps.capabilities.includes('devices.probe'));
    assert.equal((await bridge.send(allowed+'/page','devices.testPrint',{deviceId:'COM999'})).error,
      'device_not_paired_for_site');
    const fake=process.platform==='win32'?'COM999':
      process.platform==='darwin'?'/dev/cu.bridge99999':'/dev/ttyUSB99999';
    const pairing=await bridge.send(allowed+'/page','devices.pair',{
      deviceId:fake,kind:'scale',profile:'ascii-stream',baudRate:9600
    });
    assert.equal(pairing.ok,false,'Nonexistent device cannot be paired');
    assert.equal(pairing.error,'device_not_detected');
    const paired=await bridge.send(allowed+'/page','devices.paired');
    assert.equal(paired.ok,true);assert.equal(paired.devices.length,0);
    assert.equal((await bridge.send(allowed+'/page','devices.list')).ok,true);
    await content(family,bridge);
    console.log(family+': real native host + simulated extension messaging verified');
  }
  console.log('C++ framing and browser-origin/device checks passed (no hardware involved)');
  }
}finally{
  rmSync(home,{recursive:true,force:true});
}
