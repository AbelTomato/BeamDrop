import { Send } from 'lucide-react'
import { useState, type FormEvent, type ReactNode } from 'react'
import { Button } from '../../components/ui/button'
import { Input } from '../../components/ui/input'
import type { SendRequest } from '../../platform/BeamDropApi'

export function SendPanel({ onSend, busy }: { onSend: (request: SendRequest) => Promise<void>; busy: boolean }) {
  const [sourcePath, setSourcePath] = useState('')
  const [targetHost, setTargetHost] = useState('127.0.0.1')
  const [targetPort, setTargetPort] = useState('9090')
  const [error, setError] = useState<string | null>(null)
  async function submit(event: FormEvent) {
    event.preventDefault()
    const port = Number(targetPort)
    if (!sourcePath.trim() || !targetHost.trim() || !Number.isInteger(port) || port < 1 || port > 65535) { setError('请填写文件或文件夹路径、目标主机和有效端口。'); return }
    setError(null); await onSend({ sourcePath: sourcePath.trim(), targetHost: targetHost.trim(), targetPort: port })
  }
  return <section className="monument-panel p-5"><div className="mb-5"><h2 className="monument-section-title">发送文件或文件夹</h2></div><form className="grid gap-4" onSubmit={submit}><Field label="文件或文件夹路径"><Input value={sourcePath} onChange={(event) => setSourcePath(event.target.value)} placeholder="D:\\files\\example.zip 或 D:\\files\\project" /></Field><div className="grid gap-4 sm:grid-cols-[1fr_9rem]"><Field label="目标主机"><Input value={targetHost} onChange={(event) => setTargetHost(event.target.value)} placeholder="192.168.1.20" /></Field><Field label="端口"><Input inputMode="numeric" value={targetPort} onChange={(event) => setTargetPort(event.target.value)} /></Field></div>{error && <p className="text-sm text-destructive">{error}</p>}<Button className="monument-button w-full sm:w-auto" type="submit" disabled={busy}><Send size={16} />{busy ? '正在创建任务' : '发送'}</Button></form></section>
}
function Field({ label, children }: { label: string; children: ReactNode }) { return <label className="monument-field grid gap-1.5 text-sm font-medium">{label}{children}</label> }