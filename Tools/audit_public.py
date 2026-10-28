"""Inspect the exact Git index without printing matched secrets or personal values."""
from pathlib import Path
import subprocess,re,json,sys

ROOT=Path(__file__).resolve().parents[1]
def git(*args):return subprocess.check_output(['git','-C',str(ROOT),*args])
rules={
 'token':re.compile(rb'(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{30,}|sk-[A-Za-z0-9_-]{24,}|AKIA[A-Z0-9]{16})'),
 'assigned_secret':re.compile(rb'''(?i)(?:api[_-]?key|password|client[_-]?secret|access[_-]?token|security[_-]?token)\s*[=:]\s*["']?([A-Za-z0-9+/._-]{16,})["']?'''),
 'private_key':re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'),
 'windows_absolute_path':re.compile(rb'(?<![A-Za-z0-9])[A-Za-z]:[\\/]{1,2}(?:Users|UE|Chatgpt|Program Files|Windows|Git|BuildTools|[^\s"\r\n]{2,})',re.I),
 'unix_home_path':re.compile(rb'/(?:Users|home|mnt)/[^\s"\r\n]+'),
 'private_email':re.compile(rb'\b[A-Z0-9._%+-]+@(?!users\.noreply\.github\.com)[A-Z0-9.-]+\.[A-Z]{2,}\b',re.I),
}
forbidden={'.sav','.pdb','.obj','.pch','.log','.tmp','.temp','.pem','.key','.p12','.pfx','.pyc','.uasset','.umap','.fbx','.blend','.wav','.mp4'}
def main():
    # Inspect the whole prospective snapshot, including unchanged index entries.
    files=[p.decode('utf8') for p in git('ls-files','--cached','-z').split(b'\0') if p]
    issues=[];total=0
    for name in files:
        path=Path(name);parts={p.lower() for p in path.parts}
        if path.suffix.lower() in forbidden or parts&{'saved','intermediate','deriveddatacache','backups','node_modules','__pycache__'}:
            issues.append({'file':name,'type':'private_or_generated_artifact'})
        data=git('show',':'+name);total+=len(data)
        if len(data)>25*1024**2:issues.append({'file':name,'type':'oversize_blob'})
        if path.suffix.lower() not in {'.png','.jpg','.jpeg'}:
            for kind,rx in rules.items():
                count=len(rx.findall(data))
                if count:issues.append({'file':name,'type':kind,'count':count})
    report={'status':'passed' if not issues else 'failed','files':len(files),'bytes':total,'scope':'Exact staged Git blobs, not all untracked local files; regex scanning cannot prove absence of every possible secret.','issues':issues}
    print(json.dumps(report,ensure_ascii=False,indent=2));return 0 if not issues else 1
if __name__=='__main__':sys.exit(main())
