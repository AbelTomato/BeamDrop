import { cn } from '../../lib/utils'

export function Progress({ value, className }: { value: number; className?: string }) {
  return <div className={cn('h-2 overflow-hidden rounded-sm bg-[#dfe7e3]', className)}><div className="h-full bg-primary transition-[width] duration-300" style={{ width: `${Math.max(0, Math.min(100, value))}%` }} /></div>
}