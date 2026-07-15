import { cn } from '../../lib/utils'

export function Progress({ value, className }: { value: number; className?: string }) {
  const safeValue = Number.isFinite(value) ? Math.max(0, Math.min(100, value)) : 0
  return <div className={cn('h-1.5 min-w-0 w-full max-w-full overflow-hidden rounded-full bg-slate-700', className)}><div className="h-full max-w-full rounded-full bg-cyan-400 shadow-[0_0_10px_rgba(34,211,238,0.8)] transition-[width] duration-300 ease-out" style={{ width: `${safeValue}%` }} /></div>
}