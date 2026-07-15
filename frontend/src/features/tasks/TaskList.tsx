import { AlertCircle, Ban, CheckCircle2, Clock3, LoaderCircle, X } from 'lucide-react'
import { Badge } from '../../components/ui/badge'
import { Progress } from '../../components/ui/progress'
import type { TaskSnapshot } from '../../platform/BeamDropApi'

export function TaskList({ tasks, onCancel }: { tasks: readonly TaskSnapshot[]; onCancel: (taskId: string) => void }) {
  const ordered = [...tasks].sort((a, b) => b.createdAt.localeCompare(a.createdAt))
  return <section className="monument-panel monument-tasks"><div className="flex items-center justify-between border-b border-border px-5 py-4"><h2 className="monument-section-title">任务列表</h2><Badge>{ordered.length}</Badge></div>{ordered.length === 0 ? <div className="px-5 py-12 text-center text-sm text-muted-foreground">当前运行没有传输任务。</div> : <ul className="space-y-3 p-4">{ordered.map((task) => <TaskRow key={task.taskId} task={task} onCancel={onCancel} />)}</ul>}</section>
}

function TaskRow({ task, onCancel }: { task: TaskSnapshot; onCancel: (taskId: string) => void }) {
  const progress = task.progress
  const percentage = progress?.totalBytes && progress.transferredBytes !== null ? progress.transferredBytes / progress.totalBytes * 100 : progress?.currentFileTotalBytes ? progress.currentFileBytes / progress.currentFileTotalBytes * 100 : 0
  const Icon = task.state === 'completed' ? CheckCircle2 : task.state === 'failed' ? AlertCircle : task.state === 'canceled' ? Ban : task.state === 'running' || task.state === 'canceling' ? LoaderCircle : Clock3
  const cancellable = task.state === 'pending' || task.state === 'running'
  return <li className="grid gap-3 rounded-xl border border-slate-700 bg-slate-900/55 px-4 py-4 shadow-md transition-colors hover:border-slate-600"><div className="flex items-start justify-between gap-3"><div className="min-w-0"><p className="truncate text-sm font-medium text-slate-100">{task.request.sourcePath}</p><p className="mt-1 text-xs text-slate-400">{task.request.targetHost}:{task.request.targetPort}{progress?.relativePath ? ` · ${progress.relativePath}` : ''}</p></div><div className="flex items-center gap-2">{cancellable && <button type="button" className="inline-flex h-8 w-8 items-center justify-center rounded-full text-slate-500 transition-colors hover:bg-slate-700/70 hover:text-rose-400" aria-label="取消任务" title="取消任务" onClick={() => onCancel(task.taskId)}><X size={17} /></button>}<Badge className={task.state === 'failed' ? 'border-rose-400/30 bg-rose-500/10 text-rose-300' : task.state === 'completed' ? 'border-emerald-400/30 bg-emerald-500/10 text-emerald-300' : 'border-cyan-400/25 bg-cyan-500/10 text-cyan-300'}><Icon className={task.state === 'running' || task.state === 'canceling' ? 'animate-spin' : ''} size={13} />{task.state}</Badge></div></div>{progress && <div className="grid gap-1.5"><Progress value={percentage} /><p className="flex justify-between text-xs text-slate-400"><span>{formatBytes(progress.transferredBytes ?? progress.currentFileBytes)} / {formatBytes(progress.totalBytes ?? progress.currentFileTotalBytes)}</span><span className="font-mono text-cyan-300">{Math.round(percentage)}%</span></p></div>}{task.error && <p className="text-sm text-rose-300">{task.error.message}</p>}</li>
}

function formatBytes(value: number) { return value < 1024 ? `${value} B` : value < 1024 ** 2 ? `${(value / 1024).toFixed(1)} KB` : `${(value / 1024 ** 2).toFixed(1)} MB` }